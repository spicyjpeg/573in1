#!/usr/bin/env python3
# -*- coding: utf-8 -*-

__version__ = "0.3.1"
__author__  = "spicyjpeg"

import json, logging, os, re
from argparse    import ArgumentParser, Namespace
from collections import Counter, defaultdict
from dataclasses import dataclass
from operator    import methodcaller
from pathlib     import Path
from struct      import Struct
from typing      import Any, Generator, Iterable, Mapping, Sequence, Type

from _common import *

## Game list (loaded from games.json)

@dataclass
class GameEntry:
	code:   str
	region: str
	name:   str

	installCart: str | None = None
	gameCart:    str | None = None
	ioBoard:     str | None = None

	# Implement the comparison overload so sorting will work.
	def __lt__(self, entry: Any) -> bool:
		return ( self.code, self.region, self.name ) < \
			( entry.code, entry.region, entry.name )

	def __str__(self) -> str:
		return f"{self.code} {self.region}"

	def getFullName(self) -> str:
		return f"{self.name} [{self.code} {self.region}]"

	def hasSystemID(self) -> bool:
		return (self.ioBoard in SYSTEM_ID_IO_BOARDS)

class GameDB:
	def __init__(self, entries: Iterable[Mapping[str, Any]] | None = None):
		self._entries: defaultdict[str, list[GameEntry]] = defaultdict(list)

		if entries:
			for entry in entries:
				self.addEntry(entry)

	def addEntry(self, entryObj: Mapping[str, Any]):
		code:   str = entryObj["code"].strip().upper()
		region: str = entryObj["region"].strip().upper()
		name:   str = entryObj["name"]

		installCart: str | None = entryObj.get("installCart", None)
		gameCart:    str | None = entryObj.get("gameCart", None)
		ioBoard:     str | None = entryObj.get("ioBoard", None)

		if GAME_CODE_REGEX.fullmatch(code.encode("ascii")) is None:
			raise ValueError(f"invalid game code: {code}")
		if GAME_REGION_REGEX.fullmatch(region.encode("ascii")) is None:
			raise ValueError(f"invalid game region: {region}")

		entry: GameEntry = GameEntry(
			code, region, name, installCart, gameCart, ioBoard
		)

		# Store all entries indexed by their game code and first two characters
		# of the region code. This allows for quick retrieval of all revisions
		# of a game.
		self._entries[code + region[0:2]].append(entry)

	def lookup(
		self, code: str, region: str
	) -> Generator[GameEntry, None, None]:
		_code:   str = code.strip().upper()
		_region: str = region.strip().upper()

		# If only two characters of the region code are provided, match all
		# entries whose region code starts with those two characters (even if
		# longer).
		for entry in self._entries[_code + _region[0:2]]:
			if _region == entry.region[0:len(_region)]:
				yield entry

## MAME dump parser

_MAME_X76F041_STRUCT: Struct = Struct("< 4x 8s 8s 8s 8s 512s")
_MAME_X76F100_STRUCT: Struct = Struct("< 4x 8s 8s 112s")
_MAME_ZS01_STRUCT:    Struct = Struct("< 4x 8s 8s 8s 112s")

_MAME_DUMP_SIZES: Sequence[int] = (
	_MAME_X76F041_STRUCT.size, _MAME_X76F100_STRUCT.size, _MAME_ZS01_STRUCT.size
)

def parseMAMEDump(dump: bytes):
	systemID: bytes = bytes(8)
	cartID:   bytes = bytes(8)
	zsID:     bytes = bytes(8)
	config:   bytes = bytes(8)

	flags: DumpFlag = \
		DumpFlag.DUMP_PUBLIC_DATA_OK | DumpFlag.DUMP_PRIVATE_DATA_OK

	match int.from_bytes(dump[0:4], "big"):
		case 0x1955aa55:
			chipType: ChipType          = ChipType.X76F041
			_, _, dataKey, config, data = _MAME_X76F041_STRUCT.unpack(dump)

			flags |= DumpFlag.DUMP_CONFIG_OK

		case 0x1900aa55:
			chipType: ChipType     = ChipType.X76F100
			dataKey, readKey, data = _MAME_X76F100_STRUCT.unpack(dump)

			if dataKey != readKey:
				raise RuntimeError(chipType, "X76F100 dumps with different read/write keys are not supported")

		case 0x5a530001:
			chipType: ChipType       = ChipType.ZS01
			_, dataKey, config, data = _MAME_ZS01_STRUCT.unpack(dump)

			#zsID   = MAME_ZS_ID
			flags |= DumpFlag.DUMP_CONFIG_OK | DumpFlag.DUMP_ZS_ID_OK

		case _id:
			raise RuntimeError(ChipType.NONE, f"unrecognized chip ID: 0x{_id:08x}")

	#if data.find(MAME_CART_ID) >= 0:
		#cartID = MAME_CART_ID
		#flags |= DumpFlag.DUMP_HAS_CART_ID | DumpFlag.DUMP_CART_ID_OK

	#if data.find(MAME_SYSTEM_ID) >= 0:
		#systemID = MAME_SYSTEM_ID
		#flags   |= DumpFlag.DUMP_HAS_SYSTEM_ID | DumpFlag.DUMP_SYSTEM_ID_OK

	return Dump(chipType, flags, systemID, cartID, zsID, dataKey, config, data)

## Data format identification

_KNOWN_FORMATS: Sequence[tuple[str, Type, DataFlag]] = (
	(
		# Used by GCB48 (and possibly other games?)
		"region only",
		SimpleParser,
		DataFlag.DATA_HAS_PUBLIC_SECTION
	), (
		"basic (no IDs)",
		BasicParser,
		DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"basic + TID",
		BasicParser,
		DataFlag.DATA_HAS_TRACE_ID | DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"basic + SID",
		BasicParser,
		DataFlag.DATA_HAS_CART_ID | DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"basic + TID, SID",
		BasicParser,
		DataFlag.DATA_HAS_TRACE_ID | DataFlag.DATA_HAS_CART_ID
			| DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"basic + prefix, TID, SID",
		BasicParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_HAS_TRACE_ID
			| DataFlag.DATA_HAS_CART_ID | DataFlag.DATA_CHECKSUM_INVERTED
	), (
		# Used by most pre-ZS01 Bemani games
		"basic + prefix, all IDs",
		BasicParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_HAS_TRACE_ID
			| DataFlag.DATA_HAS_CART_ID | DataFlag.DATA_HAS_INSTALL_ID
			| DataFlag.DATA_HAS_SYSTEM_ID | DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"extended (no IDs)",
		ExtendedParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"extended (no IDs, alt)",
		ExtendedParser,
		DataFlag.DATA_HAS_CODE_PREFIX
	), (
		# Used by GX706
		"extended (no IDs, GX706)",
		ExtendedParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_GX706_WORKAROUND
	), (
		# Used by GE936/GK936 and all ZS01 Bemani games
		"extended + all IDs",
		ExtendedParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_HAS_TRACE_ID
			| DataFlag.DATA_HAS_CART_ID | DataFlag.DATA_HAS_INSTALL_ID
			| DataFlag.DATA_HAS_SYSTEM_ID | DataFlag.DATA_HAS_PUBLIC_SECTION
			| DataFlag.DATA_CHECKSUM_INVERTED
	)
)

def newCartParser(dump: Dump) -> Parser:
	for name, constructor, flags in reversed(_KNOWN_FORMATS):
		try:
			parser: Any = constructor(dump, flags)
		except ParserError:
			continue

		logging.debug(f"found known data format: {name}")
		return parser

	raise RuntimeError("no known data format found")

## Dump processing

def processDump(
	dump: Dump, db: GameDB, nameHint: str = ""
) -> Generator[DBEntry, None, None]:
	parser: Parser = newCartParser(dump)

	# If the parser could not find a valid game code in the dump, attempt to
	# parse it from the provided hint (filename).
	if parser.region is None:
		raise RuntimeError("can't parse game region from dump")
	if parser.code is None:
		code: re.Match | None = GAME_CODE_REGEX.search(
			nameHint.upper().encode("ascii")
		)

		if code is None:
			raise RuntimeError("can't parse game code from dump nor from filename")
		else:
			parser.code = code.group().decode("ascii")

	matches: list[GameEntry]    = sorted(db.lookup(parser.code, parser.region))
	games:   dict[str, DBEntry] = {}

	if not matches:
		raise RuntimeError(f"{parser.code} {parser.region} not found in game list")

	for game in matches:
		if game.name in games:
			continue

		# TODO: handle separate installation/game carts
		if game.hasSystemID():
			parser.flags |= DataFlag.DATA_HAS_SYSTEM_ID
		else:
			parser.flags &= ~DataFlag.DATA_HAS_SYSTEM_ID

		games[game.name] = \
			DBEntry(parser.code, parser.region, game.name, dump, parser)

		logging.info(f"imported {dump.chipType.name}: {game.name}")

	yield from games.values()

## Main

_CARTDB_PATHS: Mapping[ChipType, str] = {
	ChipType.X76F041: "x76f041.cartdb",
	ChipType.X76F100: "x76f100.cartdb",
	ChipType.ZS01:    "zs01.cartdb"
}

def createParser() -> ArgumentParser:
	parser = ArgumentParser(
		description = "Recursively scans a directory for MAME dumps and generates cartdb files."
	)
	parser.add_argument(
		"-v", "--verbose",
		action = "count",
		help   = "enable additional logging levels"
	)
	parser.add_argument(
		"-o", "--output",
		type    = Path,
		default = os.curdir,
		help    = "path to output directory (current directory by default)",
		metavar = "dir"
	)
	parser.add_argument(
		"gameList",
		type = Path,
		help = "path to JSON file containing game list"
	)
	parser.add_argument(
		"input",
		type  = Path,
		nargs = "+",
		help  = "paths to input directories"
	)

	return parser

def setupLogger(level: int | None):
	logging.basicConfig(
		format = "[{levelname:8s}] {message}",
		style  = "{",
		level  = (
			logging.WARNING,
			logging.INFO,
			logging.DEBUG
		)[min(level or 0, 2)]
	)

def main():
	parser: ArgumentParser = createParser()
	args:   Namespace      = parser.parse_args()
	setupLogger(args.verbose)

	failures: Counter[ChipType]                    = Counter()
	entries:  defaultdict[ChipType, list[DBEntry]] = defaultdict(list)

	with args.gameList.open("rt") as _file:
		gameList: Sequence[Mapping[str, Any]] = json.load(_file)

	db: GameDB = GameDB(gameList)

	for inputPath in args.input:
		for rootDir, _, files in os.walk(inputPath):
			for dumpName in files:
				path: Path        = Path(rootDir, dumpName)
				dump: Dump | None = None

				# Skip files whose size does not match any of the known dump
				# formats.
				if os.stat(path).st_size not in _MAME_DUMP_SIZES:
					logging.warning(f"ignoring {dumpName}")
					continue

				try:
					with open(path, "rb") as _file:
						dump = parseMAMEDump(_file.read())

					entries[dump.chipType].extend(
						processDump(dump, db, dumpName)
					)
				except RuntimeError as exc:
					if dump is None:
						logging.error(f"failed to import: {path}, {exc}")
					else:
						logging.error(f"failed to import {dump.chipType.name}: {path}, {exc}")
						failures[dump.chipType] += 1

	# Sort all entries and generate the cartdb files.
	for chipType, dbEntries in entries.items():
		if not dbEntries:
			logging.warning(f"DB for {chipType.name} is empty")
			continue

		dbEntries.sort()

		with open(args.output / _CARTDB_PATHS[chipType], "wb") as _file:
			for entry in dbEntries:
				_file.write(entry.serialize())

		logging.info(f"{chipType.name}: {len(dbEntries)} entries saved, {failures[chipType]} failures")

if __name__ == "__main__":
	main()
