#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# 573in1 - Copyright (C) 2022-2024 spicyjpeg
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

__version__ = "1.0.3"
__author__  = "spicyjpeg"

import json, logging
from argparse import ArgumentParser, FileType, Namespace
from pathlib  import Path
from typing   import Any

from common.analysis   import MAMENVRAMDump, getBootloaderVersion
from common.cartparser import parseCartHeader, parseROMHeader
from common.decompile  import AnalysisError
from common.gamedb     import GameInfo
from common.util       import \
	JSONFormatter, JSONGroupedArray, JSONGroupedObject, setupLogger

## Game analysis

def analyzeGame(game: GameInfo, nvramDir: Path, reanalyze: bool = False):
	dump: MAMENVRAMDump = MAMENVRAMDump(nvramDir)

	if (reanalyze or game.bootloaderVersion is None) and dump.bootloader:
		try:
			game.bootloaderVersion = getBootloaderVersion(dump.bootloader)
		except AnalysisError:
			pass

	if (reanalyze or game.rtcHeader   is None) and dump.rtcHeader:
		game.rtcHeader   = parseROMHeader(dump.rtcHeader)
	if (reanalyze or game.flashHeader is None) and dump.flashHeader:
		game.flashHeader = parseROMHeader(dump.flashHeader)

	if (reanalyze or game.installCart is None) and dump.installCart:
		game.installCart = parseCartHeader(dump.installCart)
	if (reanalyze or game.gameCart    is None) and dump.gameCart:
		game.gameCart    = parseCartHeader(dump.gameCart)

## Main

def createParser() -> ArgumentParser:
	parser = ArgumentParser(
		description = \
			"Parses a list of games in JSON format and generates a new JSON "
			"file with additional information about each game extracted from "
			"MAME ROM and NVRAM dumps.",
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

	group = parser.add_argument_group("Analysis options")
	group.add_argument(
		"-r", "--reanalyze",
		action = "store_true",
		help   = \
			"Discard any existing analysis information from the input file and "
			"rebuild it by reanalyzing the game whenever possible"
	)
	group.add_argument(
		"-k", "--keep-unanalyzed",
		action = "store_true",
		help   = \
			"Do not remove entries for games that have not been analyzed from "
			"output file"
	)

	group = parser.add_argument_group("Output options")
	group.add_argument(
		"-m", "--minify",
		action = "store_true",
		help   = "Do not pretty print output file"
	)

	group = parser.add_argument_group("File paths")
	group.add_argument(
		"dumpDir",
		type = Path,
		help = "Path to MAME NVRAM directory"
	)
	group.add_argument(
		"gameInfo",
		type = FileType("rt", encoding = "utf-8"),
		help = "Path to JSON file containing initial game list"
	)
	group.add_argument(
		"output",
		type = FileType("wt", encoding = "utf-8"),
		help = "Path to JSON file to generate"
	)

	return parser

def main():
	parser: ArgumentParser = createParser()
	args:   Namespace      = parser.parse_args()
	setupLogger(args.verbose)

	with args.gameInfo as file:
		gameInfo: dict[str, Any] = json.load(file)

	games: list[JSONGroupedObject] = []

	for initialInfo in gameInfo["games"]:
		game: GameInfo = GameInfo.fromJSONObject(initialInfo)
		code: str      = f"{game.code} {"/".join(set(game.regions))}"

		# Each entry in the initial game list may be associated with one or more
		# region codes (and thus MAME dumps). This script only analyzes one dump
		# per entry, assuming all its dumps are functionally identical and only
		# differ in the region code.
		analyzed: bool = False

		for identifier in game.identifiers:
			nvramDir: Path = args.dumpDir / identifier

			if not identifier or not nvramDir.exists():
				continue

			logging.info(f"analyzing {identifier} ({code})")
			analyzeGame(game, nvramDir, args.reanalyze)

			analyzed = True
			break

		if analyzed or args.keep_unanalyzed:
			games.append(game.toJSONObject())
		if not analyzed:
			logging.error(f"no dump found for {game.name} ({code})")

	logging.info(f"saving {len(games)} entries out of {len(gameInfo["games"])}")

	# Generate the output file, carrying over the schema path (if any) from the
	# initial game list.
	root: JSONGroupedObject = JSONGroupedObject()

	if "$schema" in gameInfo:
		root.groups.append({ "$schema": gameInfo["$schema"] })

	root.groups.append({ "games": JSONGroupedArray([ games ]) })

	with args.output as file:
		for string in JSONFormatter(args.minify).serialize(root):
			file.write(string)

if __name__ == "__main__":
	main()
