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

import re
from dataclasses import dataclass
from enum        import IntEnum, IntFlag
from struct      import Struct
from typing      import Self
from zlib        import decompress

from .util import decodeBase41

## Definitions

class ChipType(IntEnum):
	NONE    = 0
	X76F041 = 1
	X76F100 = 2
	ZS01    = 3

class DumpFlag(IntFlag):
	DUMP_HAS_SYSTEM_ID   = 1 << 0
	DUMP_HAS_CART_ID     = 1 << 1
	DUMP_CONFIG_OK       = 1 << 2
	DUMP_SYSTEM_ID_OK    = 1 << 3
	DUMP_CART_ID_OK      = 1 << 4
	DUMP_ZS_ID_OK        = 1 << 5
	DUMP_PUBLIC_DATA_OK  = 1 << 6
	DUMP_PRIVATE_DATA_OK = 1 << 7

@dataclass
class ChipSize:
	privateDataOffset: int
	privateDataLength: int
	publicDataOffset:  int
	publicDataLength:  int

	def getLength(self) -> int:
		return self.privateDataLength + self.publicDataLength

RTC_HEADER_OFFSET: int = 0x00
RTC_HEADER_LENGTH: int = 0x20

FLASH_HEADER_OFFSET:     int = 0x00
FLASH_HEADER_LENGTH:     int = 0x20
FLASH_CRC_OFFSET:        int = 0x20
FLASH_EXECUTABLE_OFFSET: int = 0x24

## Cartridge dump structure

_CART_DUMP_HEADER_STRUCT: Struct = Struct("< 8s 2B 8s 8s 8s 8s 8s")
_CART_DUMP_HEADER_MAGIC:  bytes  = b"573cdump"

_CHIP_SIZES: dict[ChipType, ChipSize] = {
	ChipType.X76F041: ChipSize( 0, 384, 384, 128),
	ChipType.X76F100: ChipSize( 0, 112,   0,   0),
	ChipType.ZS01:    ChipSize(32,  80,   0,  32)
}

_QR_STRING_REGEX: re.Pattern = \
	re.compile(r"573::([0-9A-Z+-./:]+)::", re.IGNORECASE)

@dataclass
class CartDump:
	chipType: ChipType
	flags:    DumpFlag

	systemID: bytes
	cartID:   bytes
	zsID:     bytes
	dataKey:  bytes
	config:   bytes
	data:     bytes

	def getChipSize(self) -> ChipSize:
		return _CHIP_SIZES[self.chipType]

	@staticmethod
	def fromQRString(data: str) -> Self:
		qrString: re.Match | None = _QR_STRING_REGEX.search(data)

		if qrString is None:
			raise ValueError("not a valid 573in1 QR code string")

		dump: bytearray = decodeBase41(qrString.group(1).upper())

		return CartDump.fromBinary(decompress(dump))

	@staticmethod
	def fromBinary(data: bytes | bytearray) -> Self:
		(
			magic,
			chipType,
			flags,
			systemID,
			cartID,
			zsID,
			dataKey,
			config
		) = \
			_CART_DUMP_HEADER_STRUCT.unpack_from(data, 0)

		if magic != _CART_DUMP_HEADER_MAGIC:
			raise ValueError("invalid or unsupported dump format")

		offset: int = _CART_DUMP_HEADER_STRUCT.size
		length: int = _CHIP_SIZES[chipType].getLength()

		return CartDump(
			chipType,
			flags,
			systemID,
			cartID,
			zsID,
			dataKey,
			config,
			data[offset:offset + length]
		)

	def toBinary(self) -> bytes:
		return _CART_DUMP_HEADER_STRUCT.pack(
			_CART_DUMP_HEADER_MAGIC,
			self.chipType,
			self.flags,
			self.systemID,
			self.cartID,
			self.zsID,
			self.dataKey,
			self.config
		) + self.data

## MAME NVRAM cartridge dump parser

_MAME_X76F041_DUMP_STRUCT:   Struct = Struct("< 4x 8s 8s 8s 8s 512s")
_MAME_X76F100_DUMP_STRUCT:   Struct = Struct("< 4x 8s 8s 112s")
_MAME_ZS01_DUMP_STRUCT:      Struct = Struct("< 4x 8s 8s 8s 112s")
_MAME_ZS01_OLD_DUMP_STRUCT1: Struct = Struct("< 4x 8s 8s 8s 112s 3984x")
_MAME_ZS01_OLD_DUMP_STRUCT2: Struct = Struct("< 4x 8s 8s 112s 3984x")

def parseMAMECartDump(dump: bytes | bytearray) -> CartDump:
	match int.from_bytes(dump[0:4], "big"), len(dump):
		case 0x1955aa55, _MAME_X76F041_DUMP_STRUCT.size:
			writeKey, readKey, configKey, config, data = \
				_MAME_X76F041_DUMP_STRUCT.unpack(dump)

			chipType: ChipType = ChipType.X76F041
			dataKey:  bytes    = configKey

		case 0x1900aa55, _MAME_X76F100_DUMP_STRUCT.size:
			writeKey, readKey, data = \
				_MAME_X76F100_DUMP_STRUCT.unpack(dump)

			if writeKey != readKey:
				raise RuntimeError(
					"X76F100 dumps with different read and write keys are not "
					"supported"
				)

			chipType: ChipType     = ChipType.X76F100
			dataKey:  bytes        = writeKey
			config:   bytes | None = None

			# Even though older versions of MAME emulate X76F100 cartridges for
			# games that support them, no actual X76F100 cartridges seem to
			# exist.
			raise RuntimeError("X76F100 cartridge dumps are not supported")

		case 0x5a530001, _MAME_ZS01_DUMP_STRUCT.size:
			commandKey, dataKey, config, data = \
				_MAME_ZS01_DUMP_STRUCT.unpack(dump)

			chipType: ChipType = ChipType.ZS01

		case 0x5a530001, _MAME_ZS01_OLD_DUMP_STRUCT1.size:
			commandKey, dataKey, config, data = \
				_MAME_ZS01_OLD_DUMP_STRUCT1.unpack(dump)

			chipType: ChipType = ChipType.ZS01

		case 0x5a530001, _MAME_ZS01_OLD_DUMP_STRUCT2.size:
			commandKey, dataKey, data = \
				_MAME_ZS01_OLD_DUMP_STRUCT2.unpack(dump)

			chipType: ChipType     = ChipType.ZS01
			config:   bytes | None = None

		case magic, length:
			raise RuntimeError(
				f"unknown chip type {magic:#010x}, dump length {length:#x}"
			)

	return CartDump(
		chipType,
		0
			| (DumpFlag.DUMP_CONFIG_OK if config else 0)
			| DumpFlag.DUMP_PUBLIC_DATA_OK
			| DumpFlag.DUMP_PRIVATE_DATA_OK,
		b"",
		b"",
		b"",
		dataKey,
		config or b"",
		data
	)


## Flash and RTC header dump structure

_ROM_HEADER_DUMP_HEADER_STRUCT: Struct = Struct("< H x B 8s 32s")
_ROM_HEADER_DUMP_HEADER_MAGIC:  int    = 0x573e

@dataclass
class ROMHeaderDump:
	flags: DumpFlag

	systemID: bytes
	data:     bytes

	def serialize(self) -> bytes:
		return _ROM_HEADER_DUMP_HEADER_STRUCT.pack(
			_ROM_HEADER_DUMP_HEADER_MAGIC,
			self.flags,
			self.systemID,
			self.data
		)
