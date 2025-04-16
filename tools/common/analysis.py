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

import logging, re
from collections.abc import Sequence
from dataclasses     import dataclass
from pathlib         import Path

from .cart      import *
from .decompile import AnalysisError, PSEXEAnalyzer
from .mips      import ImmInstruction, Opcode, Register, encodeADDIU, encodeJR
from .util      import InterleavedFile

## MAME NVRAM directory reader

_PCMCIA_CARD_SIZES: Sequence[int] = 8, 16, 32, 64

def _getPCMCIACardSize(path: Path, card: int) -> int | None:
	for size in _PCMCIA_CARD_SIZES:
		if (
			(path / f"pccard{card}_konami_dual_slot3_{size}mb_1l").is_file() and
			(path / f"pccard{card}_konami_dual_slot4_{size}mb_1l").is_file()
		):
			return size * 2
		if (path / f"pccard{card}_{size}mb_1l").is_file():
			return size

	return None

def _loadCartDump(path: Path) -> CartDump | None:
	try:
		with open(path, "rb") as file:
			return parseMAMECartDump(file.read())
	except FileNotFoundError:
		return None

@dataclass
class MAMENVRAMDump:
	pcmcia1Size: int | None = None
	pcmcia2Size: int | None = None

	rtcHeader:   ROMHeaderDump | None = None
	flashHeader: ROMHeaderDump | None = None
	bootloader:  PSEXEAnalyzer | None = None

	installCart: CartDump | None = None
	gameCart:    CartDump | None = None

	@staticmethod
	def fromDirectory(path: Path):
		try:
			with open(path / "m48t58", "rb") as file:
				file.seek(RTC_HEADER_OFFSET)

				rtcHeader: ROMHeaderDump | None = ROMHeaderDump(
					DumpFlag.DUMP_PUBLIC_DATA_OK,
					b"",
					file.read(RTC_HEADER_LENGTH)
				)
		except FileNotFoundError:
			rtcHeader: ROMHeaderDump | None = None

		try:
			with InterleavedFile(
				open(path / "29f016a.31m", "rb"),
				open(path / "29f016a.27m", "rb")
			) as file:
				file.seek(FLASH_HEADER_OFFSET)

				flashHeader: ROMHeaderDump | None = ROMHeaderDump(
					DumpFlag.DUMP_PUBLIC_DATA_OK,
					b"",
					file.read(FLASH_HEADER_LENGTH)
				)

				# FIXME: the executable's CRC32 should probably be validated
				file.seek(FLASH_EXECUTABLE_OFFSET)

				try:
					bootloader: PSEXEAnalyzer | None = PSEXEAnalyzer(file)
				except AnalysisError:
					bootloader: PSEXEAnalyzer | None = None
		except FileNotFoundError:
			flashHeader: ROMHeaderDump | None = None
			bootloader:  PSEXEAnalyzer | None = None

		return MAMENVRAMDump(
			_getPCMCIACardSize(path, 1),
			_getPCMCIACardSize(path, 2),
			rtcHeader,
			flashHeader,
			bootloader,
			_loadCartDump(path / "cassette_install_eeprom"),
			_loadCartDump(path / "cassette_game_eeprom")
		)

## Bootloader executable analysis

_BOOT_VERSION_REGEX: re.Pattern = \
	re.compile(rb"\0BOOT VER\.? *(1\.[0-9A-Z]+)\0")

def getBootloaderVersion(exe: PSEXEAnalyzer) -> str:
	for matched in _BOOT_VERSION_REGEX.finditer(exe.body):
		version:   bytes = matched.group(1)
		argString: bytes = b"\0" + version + b"\0"

		# A copy of the version string with no "BOOT VER" prefix is always
		# present in the launcher and passed to the game's command line.
		if argString not in exe.body:
			logging.warning("found version string with no prefix-less copy")

		return version.decode("ascii")

	raise AnalysisError("could not find version string")

## Game executable analysis

# In order to support chips from multiple manufacturers, Konami's flash and
# security cartridge drivers use vtable arrays to dispatch API calls to the
# appropriate driver. The following arrays are present in the binary:
#
#   struct {
#     int (*eraseChip)(const uint8_t *dataKey);
#     int (*setDataKey)(
#       uint8_t type, const uint8_t *oldKey, const uint8_t *newKey
#     );
#     int (*readData)(
#       const uint8_t *dataKey, uint32_t offset, void *output, size_t length
#     );
#     int (*writeData)(
#       const uint8_t *dataKey, uint32_t offset, const void *data, size_t length
#     );
#     int (*readConfig)(const uint8_t *dataKey, void *output);
#     int (*writeConfig)(const uint8_t *dataKey, const void *config);
#     int (*readDS2401)(void *output);
#     int chipType, capacity;
#   } CART_DRIVERS[4];
#
#   struct {
#     int (*eraseSector)(void *ptr);
#     int (*flushErase)(void);
#     int (*flushEraseLower)(void);
#     int (*flushEraseUpper)(void);
#     int (*writeHalfword)(void *ptr, uint16_t value);
#     int (*writeHalfwordAsync)(void *ptr, uint16_t value);
#     int (*flushWrite)(void *ptr, uint16_t value);
#     int (*flushWriteLower)(void *ptr, uint16_t value);
#     int (*flushWriteUpper)(void *ptr, uint16_t value);
#     int (*resetChip)(void *ptr);
#   } FLASH_DRIVERS[4];

def _findDriverTableCalls(
	exe:            PSEXEAnalyzer,
	dummyErrorCode: int,
	functionNames:  Sequence[str] = (),
	valueNames:     Sequence[str] = ()
) -> dict[str, int]:
	# The first entry of each array is always a dummy driver containing pointers
	# to a function that returns an error code. The table can thus be found by
	# locating the dummy function and all contiguous references to it.
	table: int = 0

	for dummy in exe.findBytes(
		encodeJR(Register.RA) +
		encodeADDIU(Register.V0, Register.ZERO, dummyErrorCode)
	):
		dummyFunctions: bytes = dummy.to_bytes(4, "little") * len(functionNames)
		dummyValues:    bytes = bytes(4 * len(valueNames))

		try:
			table = exe.findSingleMatch(dummyFunctions + dummyValues)
			break
		except AnalysisError:
			continue

	if not table:
		raise AnalysisError(
			"could not locate any valid table referenced by a dummy function"
		)

	logging.debug(f"table found at {table:#010x}")

	# Search the binary for functions that are wrappers around the driver table.
	memberNames: Sequence[str]  = functionNames + valueNames
	functions:   dict[str, int] = {}

	for offset in exe.findFunctionReturns():
		match (
			exe.disassembleAt(offset +  4),
			exe.disassembleAt(offset + 16),
			exe.disassembleAt(offset + 40)
		):
			case (
				ImmInstruction(
					opcode = Opcode.LUI,   rt = Register.V1, value = msb
				), ImmInstruction(
					opcode = Opcode.ADDIU, rt = Register.V1, value = lsb
				), ImmInstruction(
					opcode = Opcode.LW,    rt = Register.V0, value = index
				)
			) if ((msb << 16) + lsb) == table:
				index //= 4

				if (index < 0) or (index >= len(memberNames)):
					logging.debug(
						f"ignoring candidate at {offset:#010x} due to "
						f"out-of-bounds index {index}"
					)
					continue

				name: str       = memberNames[index]
				functions[name] = offset

				logging.debug(f"found {name} at {offset:#010x}")

	return functions

def findCartFunctions(exe: PSEXEAnalyzer) -> dict[str, int]:
	return _findDriverTableCalls(
		exe, -2, (
			"eraseChip",
			"setDataKey",
			"readSector",
			"writeSector",
			"readConfig",
			"writeConfig",
			"readCartID",
		), (
			"chipType",
			"capacity"
		)
	)

def findFlashFunctions(exe: PSEXEAnalyzer) -> dict[str, int]:
	return _findDriverTableCalls(
		exe, -1, (
			"eraseSector",
			"flushErase",
			"flushEraseLower",
			"flushEraseUpper",
			"writeHalfword",
			"writeHalfwordAsync",
			"flushWrite",
			"flushWriteLower",
			"flushWriteUpper",
			"resetChip"
		)
	)
