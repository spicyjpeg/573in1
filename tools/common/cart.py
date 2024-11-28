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

from dataclasses import dataclass
from enum        import IntEnum, IntFlag
from struct      import Struct
from typing      import Mapping
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

## Cartridge dump structure

_CART_DUMP_HEADER_STRUCT: Struct = Struct("< H 2B 8s 8s 8s 8s 8s")
_CART_DUMP_HEADER_MAGIC:  int    = 0x573d

_CHIP_SIZES: Mapping[ChipType, tuple[int, int, int]] = {
	ChipType.X76F041: ( 512, 384, 128 ),
	ChipType.X76F100: ( 112,   0,   0 ),
	ChipType.ZS01:    ( 112,   0,  32 )
}

_QR_STRING_START: str = "573::"
_QR_STRING_END:   str = "::"

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

	def getChipSize(self) -> tuple[int, int, int]:
		return _CHIP_SIZES[self.chipType]

	def serialize(self) -> bytes:
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

def parseCartDump(data: bytes) -> CartDump:
	magic, chipType, flags, systemID, cartID, zsID, dataKey, config = \
		_CART_DUMP_HEADER_STRUCT.unpack(data[0:_CART_DUMP_HEADER_STRUCT.size])

	if magic != _CART_DUMP_HEADER_MAGIC:
		raise ValueError(f"invalid or unsupported dump format: {magic:#04x}")

	length, _, _ = _CHIP_SIZES[chipType]

	return CartDump(
		chipType, flags, systemID, cartID, zsID, dataKey, config,
		data[_CART_DUMP_HEADER_STRUCT.size:_CART_DUMP_HEADER_STRUCT.size + length]
	)

def parseCartQRString(data: str) -> CartDump:
	_data: str = data.strip().upper()

	if not _data.startswith(_QR_STRING_START):
		raise ValueError(f"dump string does not begin with '{_QR_STRING_START}'")
	if not _data.endswith(_QR_STRING_END):
		raise ValueError(f"dump string does not end with '{_QR_STRING_END}'")

	_data = _data[len(_QR_STRING_START):-len(_QR_STRING_END)]

	return parseCartDump(decompress(decodeBase41(_data)))

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
