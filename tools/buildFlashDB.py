#!/usr/bin/env python3
# -*- coding: utf-8 -*-

__version__ = "0.4.1"
__author__  = "spicyjpeg"

import json, logging, os, re
from argparse import ArgumentParser, FileType, Namespace
from pathlib  import Path
from typing   import ByteString, Mapping, TextIO

from common.cart     import DumpFlag, ROMHeaderDump
from common.cartdata import *
from common.games    import GameDB, GameDBEntry
from common.util     import InterleavedFile

## Flash dump "parser"

_ROM_HEADER_LENGTH: int   = 0x20
_MAME_SYSTEM_ID:    bytes = bytes.fromhex("01 12 34 56 78 9a bc 3d")

def parseFlashDump(dump: bytes) -> ROMHeaderDump:
	return ROMHeaderDump(
		DumpFlag.DUMP_HAS_SYSTEM_ID | DumpFlag.DUMP_SYSTEM_ID_OK,
		_MAME_SYSTEM_ID,
		dump[0:_ROM_HEADER_LENGTH]
	)

## Dump processing

def processDump(
	dump: ROMHeaderDump, gameDB: GameDB, nameHints: Sequence[str] = [],
	exportFile: TextIO | None = None
) -> ROMHeaderDBEntry:
	parser: ROMHeaderParser = newROMHeaderParser(dump)

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

	if game.hasSystemID() and game.flashLockedToIOBoard:
		if not (parser.flags & DataFlag.DATA_HAS_SYSTEM_ID):
			raise RuntimeError("game has a system ID but dump has no signature")
	else:
		if parser.flags & DataFlag.DATA_HAS_SYSTEM_ID:
			raise RuntimeError("dump has a signature but game has no system ID")

	logging.info(f"imported: {game.getFullName()}")
	return ROMHeaderDBEntry(parser.code, parser.region, game.name, parser)

## Main

_FULL_DUMP_SIZE:     int = 0x1000000
_EVEN_ODD_DUMP_SIZE: int = 0x200000

def createParser() -> ArgumentParser:
	parser = ArgumentParser(
		description = \
			"Recursively scans a directory for subdirectories containing MAME "
			"flash dumps, analyzes them and generates .db files.",
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
		type = Path,
		help = "Path to JSON file containing game list"
	)
	group.add_argument(
		"input",
		type  = Path,
		nargs = "+",
		help  = "Paths to input directories"
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

	with args.gameList.open("rt") as _file:
		gameList: Sequence[Mapping[str, Any]] = json.load(_file)

	gameDB: GameDB = GameDB(gameList)

	failures: int                    = 0
	entries:  list[ROMHeaderDBEntry] = []

	if args.export:
		args.export.write(
			"# nameHints,code,region,matchList,formatType,flags\n"
		)

	for inputPath in args.input:
		for rootDir, _, files in os.walk(inputPath):
			root: Path = Path(rootDir)

			for dumpName in files:
				path: Path = root / dumpName
				size: int  = os.stat(path).st_size

				match path.suffix.lower():
					case ".31m":
						oddPath: Path = Path(rootDir, f"{path.stem}.27m")

						if not oddPath.is_file():
							logging.warning(f"ignoring: {path}, no .27m file")
							continue
						if size != _EVEN_ODD_DUMP_SIZE:
							logging.warning(f"ignoring: {path}, invalid size")
							continue

						with \
							open(path, "rb") as even, \
							open(oddPath, "rb") as odd:
							data: ByteString = InterleavedFile(even, odd) \
								.read(_ROM_HEADER_LENGTH)

					case ".27m":
						evenPath: Path = Path(rootDir, f"{path.stem}.31m")

						if not evenPath.is_file():
							logging.warning(f"ignoring: {path}, no .31m file")

						continue

					case _:
						if size != _FULL_DUMP_SIZE:
							logging.warning(f"ignoring: {path}, invalid size")
							continue

						with open(path, "rb") as _file:
							data: ByteString = _file.read(_ROM_HEADER_LENGTH)

				dump:  ROMHeaderDump = parseFlashDump(data)
				hints: Sequence[str] = dumpName, root.name

				try:
					entries.append(
						processDump(dump, gameDB, hints, args.export)
					)
				except RuntimeError as exc:
					logging.error(f"failed to import: {path}, {exc}")
					failures += 1

	if args.export:
		args.export.close()

	# Sort all entries and generate the .db file.
	if not entries:
		logging.warning("no entries generated")
		return

	entries.sort()

	with open(args.output / "flash.db", "wb") as _file:
		for entry in entries:
			_file.write(entry.serialize())

	logging.info(f"{len(entries)} entries saved, {failures} failures")

if __name__ == "__main__":
	main()
