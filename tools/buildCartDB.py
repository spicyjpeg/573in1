#!/usr/bin/env python3
# -*- coding: utf-8 -*-

__version__ = "0.4.2"
__author__  = "spicyjpeg"

import json, logging, os, re
from argparse    import ArgumentParser, FileType, Namespace
from collections import Counter, defaultdict
from pathlib     import Path
from struct      import Struct
from typing      import Any, Mapping, Sequence, TextIO

from common.cart     import CartDump, DumpFlag
from common.cartdata import *
from common.games    import GameDB, GameDBEntry
from common.util     import setupLogger

## MAME NVRAM file parser

_MAME_X76F041_STRUCT: Struct = Struct("< 4x 8s 8s 8s 8s 512s")
_MAME_X76F100_STRUCT: Struct = Struct("< 4x 8s 8s 112s")
_MAME_ZS01_STRUCT:    Struct = Struct("< 4x 8s 8s 8s 112s")

def parseMAMEDump(dump: bytes) -> CartDump:
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
				raise RuntimeError(
					chipType,
					"X76F100 dumps with different read/write keys are not "
					"supported"
				)

		case 0x5a530001:
			chipType: ChipType       = ChipType.ZS01
			_, dataKey, config, data = _MAME_ZS01_STRUCT.unpack(dump)

			#zsID   = _MAME_ZS_ID
			flags |= DumpFlag.DUMP_CONFIG_OK | DumpFlag.DUMP_ZS_ID_OK

		case _id:
			raise RuntimeError(
				ChipType.NONE, f"unrecognized chip ID: 0x{_id:08x}"
			)

	#if data.find(_MAME_CART_ID) >= 0:
		#cartID = _MAME_CART_ID
		#flags |= DumpFlag.DUMP_HAS_CART_ID | DumpFlag.DUMP_CART_ID_OK

	#if data.find(_MAME_SYSTEM_ID) >= 0:
		#systemID = _MAME_SYSTEM_ID
		#flags   |= DumpFlag.DUMP_HAS_SYSTEM_ID | DumpFlag.DUMP_SYSTEM_ID_OK

	return CartDump(
		chipType, flags, systemID, cartID, zsID, dataKey, config, data
	)

## Dump processing

def processDump(
	dump: CartDump, gameDB: GameDB, nameHints: Sequence[str] = [],
	exportFile: TextIO | None = None
) -> CartDBEntry:
	parser: CartParser = newCartParser(dump)

	# If the parser could not find a valid game code in the dump, attempt to
	# parse it from the provided hints.
	if parser.region is None:
		raise RuntimeError("can't parse game region from dump")

	if parser.code is None:
		for hint in nameHints:
			code: re.Match | None = GAME_CODE_REGEX.search(
				hint.upper().encode("ascii")
			)

			if code is not None:
				parser.code = code.group().decode("ascii")
				break

		if parser.code is None:
			raise RuntimeError(
				"can't parse game code from dump nor from filename"
			)

	matches: list[GameDBEntry] = sorted(
		gameDB.lookupByCode(parser.code, parser.region)
	)

	if exportFile:
		_, flags       = str(parser.flags).split(".", 1)
		matchList: str = " ".join(
			(game.mameID or f"[{game}]") for game in matches
		)

		exportFile.write(
			f"{dump.chipType.name},"
			f"{' '.join(nameHints)},"
			f"{parser.code},"
			f"{parser.region},"
			f"{matchList},"
			f"{parser.getFormatType().name},"
			f"{flags}\n"
		)
	if not matches:
		raise RuntimeError(
			f"{parser.code} {parser.region} not found in game list"
		)

	# If more than one match is found, use the first result.
	game: GameDBEntry = matches[0]

	if game.hasCartID():
		if not (parser.flags & DataFlag.DATA_HAS_CART_ID):
			raise RuntimeError("game has a cartridge ID but dump does not")
	else:
		if parser.flags & DataFlag.DATA_HAS_CART_ID:
			raise RuntimeError("dump has a cartridge ID but game does not")

	if game.hasSystemID() and game.cartLockedToIOBoard:
		if not (parser.flags & DataFlag.DATA_HAS_SYSTEM_ID):
			raise RuntimeError("game has a system ID but dump does not")
	else:
		if parser.flags & DataFlag.DATA_HAS_SYSTEM_ID:
			raise RuntimeError("dump has a system ID but game does not")

	logging.info(f"imported {dump.chipType.name}: {game.getFullName()}")
	return CartDBEntry(parser.code, parser.region, game.name, dump, parser)

## Main

_MAME_DUMP_SIZES: Sequence[int] = (
	_MAME_X76F041_STRUCT.size,
	_MAME_X76F100_STRUCT.size,
	_MAME_ZS01_STRUCT.size
)

def createParser() -> ArgumentParser:
	parser = ArgumentParser(
		description = \
			"Recursively scans a directory for MAME dumps of X76F041 and ZS01 "
			"cartridges, analyzes them and generates .db files.",
		add_help    = False
	)

	group = parser.add_argument_group("Tool options")
	group.add_argument(
		"-h", "--help",
		action = "help",
		help   = "Show this help message and exit"
	)
	group.add_argument(
		"-v", "--verbose",
		action = "count",
		help   = "Enable additional logging levels"
	)

	group = parser.add_argument_group("File paths")
	group.add_argument(
		"-o", "--output",
		type    = Path,
		default = os.curdir,
		help    = "Path to output directory (current directory by default)",
		metavar = "dir"
	)
	group.add_argument(
		"-e", "--export",
		type    = FileType("wt"),
		help    = "Export CSV table of all dumps parsed to specified path",
		metavar = "file"
	)
	group.add_argument(
		"gameList",
		type = FileType("rt"),
		help = "Path to JSON file containing game list"
	)
	group.add_argument(
		"input",
		type  = Path,
		nargs = "+",
		help  = "Paths to input directories"
	)

	return parser

def main():
	parser: ArgumentParser = createParser()
	args:   Namespace      = parser.parse_args()
	setupLogger(args.verbose)

	with args.gameList as _file:
		gameList: Sequence[Mapping[str, Any]] = json.load(_file)

	gameDB: GameDB = GameDB(gameList)

	failures: Counter[ChipType]                        = Counter()
	entries:  defaultdict[ChipType, list[CartDBEntry]] = defaultdict(list)

	if args.export:
		args.export.write(
			"# chipType,nameHints,code,region,matchList,formatType,flags\n"
		)

	for inputPath in args.input:
		for rootDir, _, files in os.walk(inputPath):
			root: Path = Path(rootDir)

			for dumpName in files:
				path: Path = root / dumpName
				size: int  = os.stat(path).st_size

				# Skip files whose size does not match any of the known dump
				# formats.
				if size not in _MAME_DUMP_SIZES:
					logging.warning(f"ignoring: {dumpName}, invalid size")
					continue

				with open(path, "rb") as _file:
					data: bytes = _file.read()

				try:
					dump: CartDump = parseMAMEDump(data)
				except RuntimeError as exc:
					logging.error(f"failed to parse: {path}, {exc}")
					continue

				hints: Sequence[str] = dumpName, root.name

				try:
					entries[dump.chipType].append(
						processDump(dump, gameDB, hints, args.export)
					)
				except RuntimeError as exc:
					logging.error(
						f"failed to import {dump.chipType.name}: {path}, {exc}"
					)
					failures[dump.chipType] += 1

	if args.export:
		args.export.close()

	# Sort all entries and generate the .db files.
	for chipType, _entries in entries.items():
		if not _entries:
			logging.warning(f"no entries generated for {chipType.name}")
			continue

		_entries.sort()

		with open(args.output / f"{chipType.name.lower()}.db", "wb") as _file:
			for entry in _entries:
				_file.write(entry.serialize())

		logging.info(
			f"{chipType.name}: {len(_entries)} entries saved, "
			f"{failures[chipType]} failures"
		)

if __name__ == "__main__":
	main()
