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

import numpy
from numpy   import ndarray
from PIL     import Image
from .gamedb import GAME_INFO_STRUCT, GameInfo
from .util   import \
	HashTableBuilder, StringBlobBuilder, colorFromString, hashData, \
	roundUpToMultiple

## Simple image quantizer

# Pillow's built-in quantize() method will use different algorithms, some of
# which are broken, depending on whether the input image has an alpha channel.
# As a workaround, non-indexed-color images are "quantized" (with no dithering -
# images with more colors than allowed are rejected) manually instead.
def toIndexedImage(imageObj: Image.Image, numColors: int) -> Image.Image:
	if imageObj.mode == "P":
		return imageObj
	if imageObj.mode not in ( "RGB", "RGBA" ):
		imageObj = imageObj.convert("RGBA")

	image: ndarray = numpy.asarray(imageObj, "B")
	clut, image    = numpy.unique(
		image.reshape(( imageObj.width * imageObj.height, image.shape[2] )),
		return_inverse = True,
		axis           = 0
	)

	if clut.shape[0] > numColors:
		raise RuntimeError(
			f"source image contains {clut.shape[0]} unique colors (must be "
			f"{numColors} or less)"
		)

	image = image.astype("B").reshape(( imageObj.height, imageObj.width ))

	newImageObj: Image.Image = Image.fromarray(image, "P")

	newImageObj.putpalette(clut, imageObj.mode)
	return newImageObj

## .TIM image converter

_TIM_HEADER_STRUCT:  Struct = Struct("< 2I")
_TIM_SECTION_STRUCT: Struct = Struct("< I 4H")
_TIM_HEADER_VERSION: int    = 0x10

_LOWER_ALPHA_BOUND: int = 0x20
_UPPER_ALPHA_BOUND: int = 0xe0

# Color 0x0000 is interpreted by the PS1 GPU as fully transparent, so black
# pixels must be changed to dark gray to prevent them from becoming transparent.
_TRANSPARENT_COLOR: int = 0x0000
_BLACK_COLOR:       int = 0x0421

def _to16bpp(inputData: ndarray) -> ndarray:
	source: ndarray = inputData.astype("<H")

	r:    ndarray = ((source[:, :, 0] * 249) + 1014) >> 11
	g:    ndarray = ((source[:, :, 1] * 249) + 1014) >> 11
	b:    ndarray = ((source[:, :, 2] * 249) + 1014) >> 11
	data: ndarray = r | (g << 5) | (b << 10)

	data = numpy.where(data != _TRANSPARENT_COLOR, data, _BLACK_COLOR)

	if source.shape[2] == 4:
		alpha: ndarray = source[:, :, 3]

		data = numpy.select(
			(
				alpha > _UPPER_ALPHA_BOUND, # Leave as-is
				alpha > _LOWER_ALPHA_BOUND  # Set semitransparency flag
			), (
				data,
				data | (1 << 15)
			),
			_TRANSPARENT_COLOR
		)

	return data.reshape(source.shape[:-1])

def convertIndexedImage(imageObj: Image.Image) -> tuple[ndarray, ndarray]:
	# Pillow has basically no support at all for palette manipulation, so nasty
	# hacks are required here.
	colorDepth: int = {
		"RGB":  3,
		"RGBA": 4
	}[imageObj.palette.mode]

	clut:      ndarray = numpy.frombuffer(imageObj.palette.tobytes(), "B")
	numColors: int     = clut.shape[0] // colorDepth

	# Pad the palette to 16 or 256 colors after converting it to 16bpp.
	clut           = _to16bpp(clut.reshape(( 1, numColors, colorDepth )))
	padAmount: int = (16 if (numColors <= 16) else 256) - numColors

	if padAmount:
		clut = numpy.c_[ clut, numpy.zeros(( 1, padAmount ), "<H") ]

	image: ndarray = numpy.asarray(imageObj, "B")

	if image.shape[1] % 2:
		image = numpy.c_[ image, numpy.zeros((imageObj.height, 1 ), "B") ]

	# Pack two pixels into each byte for 4bpp images.
	if numColors <= 16:
		image = image[:, 0::2] | (image[:, 1::2] << 4)

		if image.shape[1] % 2:
			image = numpy.c_[ image, numpy.zeros(( imageObj.height, 1 ), "B") ]

	return image, clut

def generateIndexedTIM(
	imageObj: Image.Image,
	ix:       int,
	iy:       int,
	cx:       int,
	cy:       int
) -> bytearray:
	if (ix < 0) or (ix > 1023) or (iy < 0) or (iy > 1023):
		raise ValueError("image X/Y coordinates must be in 0-1023 range")
	if (cx < 0) or (cx > 1023) or (cy < 0) or (cy > 1023):
		raise ValueError("palette X/Y coordinates must be in 0-1023 range")

	image, clut     = convertIndexedImage(imageObj)
	data: bytearray = bytearray()

	data += _TIM_HEADER_STRUCT.pack(
		_TIM_HEADER_VERSION,
		0x8 if (clut.size <= 16) else 0x9
	)

	data += _TIM_SECTION_STRUCT.pack(
		_TIM_SECTION_STRUCT.size + clut.size * 2,
		cx,
		cy,
		clut.shape[1],
		clut.shape[0]
	)
	data.extend(clut)

	data += _TIM_SECTION_STRUCT.pack(
		_TIM_SECTION_STRUCT.size + image.size,
		ix,
		iy,
		image.shape[1] // 2,
		image.shape[0]
	)
	data.extend(image)

	return data

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

	metrics: bytearray = bytearray()
	metrics           += _METRICS_HEADER_STRUCT.pack(
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
			metrics += bytes(_METRICS_ENTRY_STRUCT.size)
		else:
			metrics += _METRICS_ENTRY_STRUCT.pack(
				entry.fullHash | (entry.chainIndex << 21),
				entry.data
			)

	return metrics

## Color palette generator

_PALETTE_ENTRY_STRUCT: Struct = Struct("< 3B x")

_PALETTE_ENTRIES: Sequence[str] = (
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

_GAMEDB_SORT_ORDERS: Sequence[Callable[[ GameInfo ], tuple]] = (
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
		gameListData += game.toBinary(blobOffset + blob.addString(name))

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
	numBuckets: int = 256
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
