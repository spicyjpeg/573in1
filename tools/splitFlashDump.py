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

__version__ = "0.3.4"
__author__  = "spicyjpeg"

import os
from argparse        import ArgumentParser, Namespace
from collections.abc import ByteString, Sequence
from pathlib         import Path
from shutil          import copyfile

## Flash dump splitting

_FLASH_BANKS:      Sequence[str] = "m", "l", "j", "h"
_FLASH_BANK_SIZE:  int           = 0x400000
_PCMCIA_BANK_SIZE: int           = 0x400000

def splitFlash(inputPath: Path, outputPath: Path):
	with open(inputPath, "rb") as file:
		for bank in _FLASH_BANKS:
			with \
				open(outputPath / f"29f016a.31{bank}", "wb") as even, \
				open(outputPath / f"29f016a.27{bank}", "wb") as odd:
				data: ByteString = file.read(_FLASH_BANK_SIZE)

				even.write(data[0::2])
				odd.write(data[1::2])

def splitPCMCIACard(inputPath: Path, outputPath: Path, card: int, size: int):
	name: str = f"pccard{card}_{size // 0x100000}mb"

	with open(inputPath, "rb") as file:
		for bank in range(1, (size // _PCMCIA_BANK_SIZE) + 1):
			with \
				open(outputPath / f"{name}_{bank}l", "wb") as even, \
				open(outputPath / f"{name}_{bank}u", "wb") as odd:
				data: ByteString = file.read(_PCMCIA_BANK_SIZE)

				even.write(data[0::2])
				odd.write(data[1::2])

## Main

_PCMCIA_CARD_SIZE: int = 32

def createParser() -> ArgumentParser:
	parser = ArgumentParser(
		description = \
			"Converts a set of dump files generated by the tool (rtc.bin, "
			"flash.bin, pcmcia*.bin) to NVRAM files as used by MAME.",
		add_help    = False
	)

	group = parser.add_argument_group("Tool options")
	group.add_argument(
		"-h", "--help",
		action = "help",
		help   = "Show this help message and exit"
	)

	group = parser.add_argument_group("PCMCIA card options")
	for card in ( 1, 2 ):
		group.add_argument(
			f"-{card}", f"--pcmcia{card}-size",
			type    = lambda value: int(value, 0),
			default = _PCMCIA_CARD_SIZE,
			help    = \
				f"Set size of PCMCIA card in slot {card} in megabytes (default "
				f"{_PCMCIA_CARD_SIZE})",
			metavar = "value"
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
		"input",
		type = Path,
		help = "Path to directory containing dumped files"
	)

	return parser

def main():
	parser: ArgumentParser = createParser()
	args:   Namespace      = parser.parse_args()

	converted: bool = False

	#if os.path.isfile(args.input / "bios.bin"):
		#copyfile(args.input / "bios.bin", args.output / "700a01.22g")
		#converted = True

	if os.path.isfile(args.input / "rtc.bin"):
		copyfile(args.input / "rtc.bin", args.output / "m48t58")
		converted = True

	if os.path.isfile(args.input / "flash.bin"):
		splitFlash(args.input / "flash.bin", args.output)
		converted = True

	for card, size in enumerate(( args.pcmcia1_size, args.pcmcia2_size ), 1):
		path: Path = args.input / f"pcmcia{card}.bin"

		if os.path.isfile(path):
			splitPCMCIACard(path, args.output, card, size * 0x100000)
			converted = True

	if not converted:
		parser.error("no suitable files to convert found in input directory")

if __name__ == "__main__":
	main()
