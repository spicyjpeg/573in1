# -*- coding: utf-8 -*-

# 573in1 - Copyright (C) 2022-2025 spicyjpeg
#
# 573in1 is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# 573in1 is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# 573in1. If not, see <https://www.gnu.org/licenses/>.

from collections.abc import Generator, Mapping, Sequence
from dataclasses     import dataclass
from struct          import Struct
from typing          import Any, Callable

from .gamedb import GAME_INFO_STRUCT, GameInfo
from .util   import \
	HashTableBuilder, StringBlobBuilder, colorFromString, hashData, \
	roundUpToMultiple

## Font metrics generator

_METRICS_HEADER_STRUCT: Struct = Struct("< 8s 3B b 2H")
_METRICS_HEADER_MAGIC:  bytes  = b"573fmetr"
_METRICS_ENTRY_STRUCT:  Struct = Struct("< 2I")

def generateFontMetrics(
	metrics:    Mapping[str, Any],
	numBuckets: int = 256
) -> bytearray:
	spaceWidth:     int = int(metrics["spaceWidth"])
	tabWidth:       int = int(metrics["tabWidth"])
	lineHeight:     int = int(metrics["lineHeight"])
	baselineOffset: int = int(metrics["baselineOffset"])

	hashTable: HashTableBuilder = HashTableBuilder(numBuckets)

	for ch, entry in metrics["characterSizes"].items():
		x: int  = int(entry["x"])
		y: int  = int(entry["y"])
		w: int  = int(entry["width"])
		h: int  = int(entry["height"])
		i: bool = bool(entry.get("icon", False))

		if (x < 0) or (x > 255) or (y < 0) or (y > 255):
			raise ValueError("all X/Y coordinates must be in 0-255 range")
		if (w < 0) or (w > 127) or (h < 0) or (h > 127):
			raise ValueError("all characters must be <=127x127 pixels")
		if h > lineHeight:
			raise ValueError("character height exceeds line height")

		hashTable.addEntry(ord(ch), 0
			| (x <<  0)
			| (y <<  8)
			| (w << 16)
			| (h << 23)
			| (i << 30)
		)

	data: bytearray = bytearray()
	data           += _METRICS_HEADER_STRUCT.pack(
		_METRICS_HEADER_MAGIC,
		spaceWidth,
		tabWidth,
		lineHeight,
		baselineOffset,
		numBuckets,
		len(hashTable.entries)
	)

	for entry in hashTable.entries:
		if entry is None:
			data += bytes(_METRICS_ENTRY_STRUCT.size)
		else:
			data += _METRICS_ENTRY_STRUCT.pack(
				entry.fullHash | (entry.chainIndex << 21),
				entry.data
			)

	return data

## Color palette generator

_PALETTE_ENTRY_STRUCT: Struct = Struct("< 3B x")

_PALETTE_ENTRIES: tuple[str, ...] = (
	"default",
	"shadow",
	"backdrop",
	"accent1",
	"accent2",
	"window1",
	"window2",
	"window3",
	"highlight1",
	"highlight2",
	"progress1",
	"progress2",
	"box1",
	"box2",
	"text1",
	"text2",
	"title",
	"subtitle"
)

def generateColorPalette(
	palette: Mapping[str, str | Sequence[int]]
) -> bytearray:
	data: bytearray = bytearray()

	for entry in _PALETTE_ENTRIES:
		color: str | Sequence[int] | None = palette.get(entry, None)

		if color is None:
			raise ValueError(f"no entry found for {entry}")
		if isinstance(color, str):
			r, g, b = colorFromString(color)
		else:
			r, g, b = color

		data += _PALETTE_ENTRY_STRUCT.pack(r, g, b)

	return data

## String table generator

_STRING_TABLE_HEADER_STRUCT: Struct = Struct("< 8s 2H")
_STRING_TABLE_HEADER_MAGIC:  bytes  = b"573strng"
_STRING_TABLE_ENTRY_STRUCT:  Struct = Struct("< I 2H")
_STRING_TABLE_ALIGNMENT:     int    = 4

def _walkStringTree(
	strings: Mapping[str, Any],
	prefix:  str = ""
) -> Generator[tuple[int, bytes], None, None]:
	for key, value in strings.items():
		fullKey: str = prefix + key
		keyHash: int = hashData(fullKey.encode("ascii"))

		if isinstance(value, str):
			yield keyHash, value.encode("utf-8") + b"\0"
		else:
			yield from _walkStringTree(value, f"{fullKey}.")

def generateStringTable(
	strings:    Mapping[str, Any],
	numBuckets: int = 256
) -> bytearray:
	hashTable: HashTableBuilder  = HashTableBuilder(numBuckets)
	blob:      StringBlobBuilder = StringBlobBuilder(_STRING_TABLE_ALIGNMENT)

	for keyHash, string in _walkStringTree(strings):
		hashTable.addEntry(keyHash, blob.addString(string))

	blobOffset: int = 0 \
		+ _STRING_TABLE_HEADER_STRUCT.size \
		+ _STRING_TABLE_ENTRY_STRUCT.size * len(hashTable.entries)

	tableData: bytearray = bytearray()
	tableData           += _STRING_TABLE_HEADER_STRUCT.pack(
		_STRING_TABLE_HEADER_MAGIC,
		numBuckets,
		len(hashTable.entries)
	)

	for entry in hashTable.entries:
		if entry is None:
			tableData += bytes(_STRING_TABLE_ENTRY_STRUCT.size)
		else:
			tableData += _STRING_TABLE_ENTRY_STRUCT.pack(
				entry.fullHash,
				blobOffset + entry.data,
				entry.chainIndex
			)

	return tableData + blob.data

## Game database generator

_GAMEDB_HEADER_STRUCT:    Struct = Struct("< 8s 2H")
_GAMEDB_HEADER_MAGIC:     bytes  = b"573gmedb"
_GAMEDB_STRING_ALIGNMENT: int    = 4

_GAMEDB_SORT_ORDERS: tuple[Callable[[ GameInfo ], tuple], ...] = (
	lambda game: ( game.code,         game.name ),            # SORT_CODE
	lambda game: ( game.name,         game.code ),            # SORT_NAME
	lambda game: ( game.series or "", game.code, game.name ), # SORT_SERIES
	lambda game: ( game.year,         game.code, game.name )  # SORT_YEAR
)

def generateGameDB(gamedb: Mapping[str, Any]) -> bytearray:
	numEntries:     int = len(gamedb["games"])
	gameListOffset: int = 0 \
		+ _GAMEDB_HEADER_STRUCT.size \
		+ len(_GAMEDB_SORT_ORDERS) * 2 * numEntries
	blobOffset:     int = 0 \
		+ gameListOffset \
		+ GAME_INFO_STRUCT.size * numEntries

	games: list[GameInfo]    = []
	blob:  StringBlobBuilder = StringBlobBuilder(_GAMEDB_STRING_ALIGNMENT)

	gameListData:  bytearray = bytearray()
	sortTableData: bytearray = bytearray()
	sortTableData           += _GAMEDB_HEADER_STRUCT.pack(
		_GAMEDB_HEADER_MAGIC,
		numEntries,
		len(_GAMEDB_SORT_ORDERS)
	)

	for info in gamedb["games"]:
		game: GameInfo = GameInfo.fromJSONObject(info)
		name: bytes    = game.name.encode("utf-8") + b"\0"

		games.append(game)
		gameListData += game.serialize(blobOffset + blob.addString(name))

	for sortOrder in _GAMEDB_SORT_ORDERS:
		indices: list[int] = \
			sorted(range(numEntries), key = lambda i: sortOrder(games[i]))

		for index in indices:
			offset: int    = gameListOffset + GAME_INFO_STRUCT.size * index
			sortTableData += offset.to_bytes(2, "little")

	return sortTableData + gameListData + blob.data

## Package header generator

_PACKAGE_INDEX_HEADER_STRUCT: Struct = Struct("< 8s I 2H")
_PACKAGE_INDEX_HEADER_MAGIC:  bytes  = b"573packg"
_PACKAGE_INDEX_ENTRY_STRUCT:  Struct = Struct("< I 2H Q 2I")
_PACKAGE_STRING_ALIGNMENT:    int    = 4

@dataclass
class PackageIndexEntry:
	offset:       int
	compLength:   int
	uncompLength: int
	nameOffset:   int = 0

def generatePackageIndex(
	files:      Mapping[str, PackageIndexEntry],
	alignment:  int = 2048,
	numBuckets: int = 64
) -> bytearray:
	hashTable: HashTableBuilder  = HashTableBuilder(numBuckets)
	blob:      StringBlobBuilder = StringBlobBuilder(_PACKAGE_STRING_ALIGNMENT)

	for name, entry in files.items():
		nameString: bytes             = name.encode("ascii")
		data:       PackageIndexEntry = PackageIndexEntry(
			entry.offset,
			entry.compLength,
			entry.uncompLength,
			blob.addString(nameString + b"\0")
		)

		hashTable.addEntry(hashData(nameString), data)

	tableLength: int = 0 \
		+ _PACKAGE_INDEX_HEADER_STRUCT.size \
		+ _PACKAGE_INDEX_ENTRY_STRUCT.size * len(hashTable.entries)
	indexLength: int = tableLength + len(blob.data)

	tableData: bytearray = bytearray()
	tableData           += _PACKAGE_INDEX_HEADER_STRUCT.pack(
		_PACKAGE_INDEX_HEADER_MAGIC,
		indexLength,
		numBuckets,
		len(hashTable.entries)
	)

	fileDataOffset: int = roundUpToMultiple(indexLength, alignment)

	for entry in hashTable.entries:
		if entry is None:
			tableData += bytes(_PACKAGE_INDEX_ENTRY_STRUCT.size)
		else:
			tableData += _PACKAGE_INDEX_ENTRY_STRUCT.pack(
				entry.fullHash,
				tableLength + entry.data.nameOffset,
				entry.chainIndex,
				fileDataOffset + entry.data.offset,
				entry.data.compLength,
				entry.data.uncompLength
			)

	return tableData + blob.data
