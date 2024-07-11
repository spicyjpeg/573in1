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

import logging
from dataclasses import dataclass
from enum        import IntEnum, IntFlag
from struct      import Struct, unpack
from typing      import Any, Sequence, Type

from .cart  import CartDump, ChipType, ROMHeaderDump
from .games import GAME_CODE_REGEX, GAME_REGION_REGEX
from .util  import checksum8, checksum16, shortenedMD5

## Definitions

class FormatType(IntEnum):
	BLANK    = 0
	SIMPLE   = 1
	BASIC    = 2
	EXTENDED = 3

class TraceIDType(IntEnum):
	TID_NONE             = 0
	TID_81               = 1
	TID_82_BIG_ENDIAN    = 2
	TID_82_LITTLE_ENDIAN = 3

class DataFlag(IntFlag):
	DATA_HAS_CODE_PREFIX    = 1 << 0
	DATA_HAS_TRACE_ID       = 1 << 1
	DATA_HAS_CART_ID        = 1 << 2
	DATA_HAS_INSTALL_ID     = 1 << 3
	DATA_HAS_SYSTEM_ID      = 1 << 4
	DATA_HAS_PUBLIC_SECTION = 1 << 5
	DATA_CHECKSUM_INVERTED  = 1 << 6
	DATA_GX706_WORKAROUND   = 1 << 7

class ParserError(BaseException):
	pass

## Common data structures

@dataclass
class IdentifierSet:
	traceID:   bytes | None = None # aka TID
	cartID:    bytes | None = None # aka SID
	installID: bytes | None = None # aka MID
	systemID:  bytes | None = None # aka XID

	def __init__(self, data: bytes):
		ids: list[bytes | None] = []

		for offset in range(0, 32, 8):
			_id: bytes = data[offset:offset + 8]
			ids.append(_id if sum(_id) else None)

		(
			self.traceID,
			self.cartID,
			self.installID,
			self.systemID
		) = ids

	def getFlags(self) -> DataFlag:
		flags: DataFlag = DataFlag(0)

		if self.traceID:
			flags |= DataFlag.DATA_HAS_TRACE_ID
		if self.cartID:
			flags |= DataFlag.DATA_HAS_CART_ID
		if self.installID:
			flags |= DataFlag.DATA_HAS_INSTALL_ID
		if self.systemID:
			flags |= DataFlag.DATA_HAS_SYSTEM_ID

		return flags

	def getCartIDChecksum(self, param: int) -> int:
		if self.cartID is None:
			return 0

		checksum: int = 0

		for i in range(6):
			value: int = self.cartID[i + 1]

			for j in range(i * 8, (i + 1) * 8):
				if value & 1:
					checksum ^= 1 << (j % param)

				value >>= 1

		return checksum & 0xffff

	def getTraceIDType(self, param: int) -> TraceIDType:
		if self.traceID is None:
			return TraceIDType.TID_NONE

		match self.traceID[0]:
			case 0x81:
				return TraceIDType.TID_81

			case 0x82:
				checksum: int = self.getCartIDChecksum(param)
				big:      int = unpack("> H", self.traceID[1:3])[0]
				little:   int = unpack("< H", self.traceID[1:3])[0]

				if checksum == big:
					return TraceIDType.TID_82_BIG_ENDIAN
				elif checksum == little:
					return TraceIDType.TID_82_LITTLE_ENDIAN

				raise ValueError(
					f"trace ID mismatch, exp=0x{checksum:04x}, "
					f"big=0x{big:04x}, little=0x{little:04x}"
				)

			case prefix:
				raise ValueError(f"unknown trace ID prefix: 0x{prefix:02x}")

@dataclass
class PublicIdentifierSet:
	installID: bytes | None = None # aka MID
	systemID:  bytes | None = None # aka XID

	def __init__(self, data: bytes):
		ids: list[bytes | None] = []

		for offset in range(0, 16, 8):
			_id: bytes = data[offset:offset + 8]
			ids.append(_id if sum(_id) else None)

		self.installID, self.systemID = ids

	def getFlags(self) -> DataFlag:
		flags: DataFlag = DataFlag(0)

		if self.installID:
			flags |= DataFlag.DATA_HAS_INSTALL_ID
		if self.systemID:
			flags |= DataFlag.DATA_HAS_SYSTEM_ID

		return flags

## Cartridge data parsers

_BASIC_HEADER_STRUCT:    Struct = Struct("< 2s 2s B 3x")
_EXTENDED_HEADER_STRUCT: Struct = Struct("< 8s H 4s H")

# The system and install IDs are excluded from validation as they may not be
# always present.
_IDENTIFIER_FLAG_MASK: DataFlag = \
	DataFlag.DATA_HAS_TRACE_ID | DataFlag.DATA_HAS_CART_ID

def _getPublicData(
	dump: CartDump, flags: DataFlag, maxLength: int = 512
) -> bytes:
	if flags & DataFlag.DATA_HAS_PUBLIC_SECTION:
		_, offset, length = dump.getChipSize()

		return dump.data[offset:offset + min(length, maxLength)]
	else:
		return dump.data[0:maxLength]

@dataclass
class CartParser:
	flags:             DataFlag
	identifiers:       IdentifierSet
	publicIdentifiers: PublicIdentifierSet

	region:     str | None = None
	codePrefix: str | None = None
	code:       str | None = None
	year:       int | None = None

	def getFormatType(self) -> FormatType:
		return FormatType.BLANK

class SimpleCartParser(CartParser):
	def __init__(self, dump: CartDump, flags: DataFlag):
		region: bytes = _getPublicData(dump, flags, 8).rstrip(b"\0")

		if GAME_REGION_REGEX.fullmatch(region) is None:
			raise ParserError(f"invalid game region: {region}")

		super().__init__(
			flags,
			IdentifierSet(b""),
			PublicIdentifierSet(b""),
			region.decode("ascii")
		)

	def getFormatType(self) -> FormatType:
		return FormatType.SIMPLE

class BasicCartParser(CartParser):
	def __init__(self, dump: CartDump, flags: DataFlag):
		data: bytes = _getPublicData(dump, flags, _BASIC_HEADER_STRUCT.size)

		pri: IdentifierSet = IdentifierSet(dump.data[_BASIC_HEADER_STRUCT.size:])

		region, codePrefix, checksum = _BASIC_HEADER_STRUCT.unpack(data)

		codePrefix: bytes = codePrefix.rstrip(b"\0")
		value:      int   = checksum8(
			data[0:4], bool(flags & DataFlag.DATA_CHECKSUM_INVERTED)
		)

		if value != checksum:
			raise ParserError(
				f"invalid header checksum, exp=0x{value:02x}, "
				f"got=0x{checksum:02x}"
			)
		if GAME_REGION_REGEX.fullmatch(region) is None:
			raise ParserError(f"invalid game region: {region}")
		if bool(flags & DataFlag.DATA_HAS_CODE_PREFIX) != bool(codePrefix):
			raise ParserError(
				f"game code prefix should{' not' if codePrefix else ''} be "
				f"present"
			)
		if (pri.getFlags() ^ flags) & _IDENTIFIER_FLAG_MASK:
			raise ParserError("identifier flags do not match")

		super().__init__(
			flags,
			pri,
			PublicIdentifierSet(b""),
			region.decode("ascii"),
			codePrefix.decode("ascii") or None
		)

	def getFormatType(self) -> FormatType:
		return FormatType.BASIC

class ExtendedCartParser(CartParser):
	def __init__(self, dump: CartDump, flags: DataFlag):
		data:   bytes = \
			_getPublicData(dump, flags, _EXTENDED_HEADER_STRUCT.size + 16)
		idsPri: bytes = dump.data[_EXTENDED_HEADER_STRUCT.size + 16:]
		idsPub: bytes = dump.data[_EXTENDED_HEADER_STRUCT.size:]
		header: bytes = data[0:_EXTENDED_HEADER_STRUCT.size]

		pri: IdentifierSet       = IdentifierSet(idsPri)
		pub: PublicIdentifierSet = PublicIdentifierSet(idsPub)

		if flags & DataFlag.DATA_GX706_WORKAROUND:
			data = data[0:1] + b"X" + data[2:]

		code, year, region, checksum = _EXTENDED_HEADER_STRUCT.unpack(header)

		code:   bytes = code.rstrip(b"\0")
		region: bytes = region.rstrip(b"\0")
		value:  int   = checksum16(
			data[0:14], bool(flags & DataFlag.DATA_CHECKSUM_INVERTED)
		)

		if value != checksum:
			raise ParserError(
				f"invalid header checksum, exp=0x{value:04x}, "
				f"got=0x{checksum:04x}"
			)
		if GAME_CODE_REGEX.fullmatch(code) is None:
			raise ParserError(f"invalid game code: {code}")
		if GAME_REGION_REGEX.fullmatch(region) is None:
			raise ParserError(f"invalid game region: {region}")

		if (pri.getFlags() ^ flags) & _IDENTIFIER_FLAG_MASK:
			raise ParserError("identifier flags do not match")

		_code: str = code.decode("ascii")
		super().__init__(
			flags,
			pri,
			pub,
			region.decode("ascii"),
			_code[0:2],
			_code,
			year
		)

	def getFormatType(self) -> FormatType:
		return FormatType.EXTENDED

## Flash and RTC header parsers/writers

# Used alongside the system ID and the header itself to calculate the MD5 used
# as a header signature. Seems to be the same in all games.
_SIGNATURE_SALT: bytes = bytes.fromhex("c1 a2 03 d6 ab 70 85 5e")

@dataclass
class ROMHeaderParser:
	flags:     DataFlag
	signature: bytes | None = None

	region:     str | None = None
	codePrefix: str | None = None
	code:       str | None = None
	year:       int | None = None

	def getFormatType(self) -> FormatType:
		return FormatType.BLANK

class ExtendedROMHeaderParser(ROMHeaderParser):
	def __init__(self, dump: ROMHeaderDump, flags: DataFlag):
		data:      bytes = dump.data[0:_EXTENDED_HEADER_STRUCT.size + 8]
		header:    bytes = data[0:_EXTENDED_HEADER_STRUCT.size]
		signature: bytes = data[_EXTENDED_HEADER_STRUCT.size:]

		if flags & DataFlag.DATA_GX706_WORKAROUND:
			data = data[0:1] + b"X" + data[2:]

		code, year, region, checksum = _EXTENDED_HEADER_STRUCT.unpack(header)

		code:   bytes = code.rstrip(b"\0")
		region: bytes = region.rstrip(b"\0")
		value:  int   = checksum16(
			data[0:14], bool(flags & DataFlag.DATA_CHECKSUM_INVERTED)
		)

		if value != checksum:
			raise ParserError(
				f"invalid header checksum, exp=0x{value:04x}, "
				f"got=0x{checksum:04x}"
			)
		if GAME_CODE_REGEX.fullmatch(code) is None:
			raise ParserError(f"invalid game code: {code}")
		if GAME_REGION_REGEX.fullmatch(region) is None:
			raise ParserError(f"invalid game region: {region}")

		if flags & DataFlag.DATA_HAS_SYSTEM_ID:
			expected: bytearray = \
				shortenedMD5(dump.systemID + header + _SIGNATURE_SALT)

			if signature != expected:
				raise ParserError(
					f"invalid signature, exp={expected.hex()}, "
					f"got={signature.hex()}"
				)
		else:
			if sum(signature) not in ( 0, 0xff * 8 ):
				raise ParserError("unexpected signature present")

		_code: str = code.decode("ascii")
		super().__init__(
			flags, signature, region.decode("ascii"), _code[0:2], _code, year
		)

	def getFormatType(self) -> FormatType:
		return FormatType.EXTENDED

## Cartridge and flash header database

_CART_DB_ENTRY_STRUCT:       Struct = Struct("< 6B H 8s 8s 8s 96s")
_ROM_HEADER_DB_ENTRY_STRUCT: Struct = Struct("< 2B H 8s 8s 96s")

_TRACE_ID_PARAMS: Sequence[int] = 16, 14

@dataclass
class CartDBEntry:
	code:    str
	region:  str
	name:    str
	dataKey: bytes

	chipType:    ChipType
	formatType:  FormatType
	traceIDType: TraceIDType
	flags:       DataFlag

	traceIDParam:    int = 0
	installIDPrefix: int = 0
	year:            int = 0

	def __init__(
		self, code: str, region: str, name: str, dump: CartDump,
		parser: CartParser
	):
		# Find the correct parameters for the trace ID heuristically.
		_type: TraceIDType | None = None

		for self.traceIDParam in _TRACE_ID_PARAMS:
			try:
				_type = parser.identifiers.getTraceIDType(self.traceIDParam)
			except ValueError:
				continue

			break

		if _type is None:
			raise RuntimeError("failed to determine trace ID parameters")

		self.code        = code
		self.region      = region
		self.name        = name
		self.dataKey     = dump.dataKey
		self.chipType    = dump.chipType
		self.formatType  = parser.getFormatType()
		self.traceIDType = _type
		self.flags       = parser.flags
		self.year        = parser.year or 0

		if parser.publicIdentifiers.installID is not None:
			self.installIDPrefix = parser.publicIdentifiers.installID[0]
		elif parser.identifiers.installID is not None:
			self.installIDPrefix = parser.identifiers.installID[0]
		else:
			self.installIDPrefix = 0

	# Implement the comparison overload so sorting will work. The 3-digit number
	# in the game code is used as a key.
	def __lt__(self, entry: Any) -> bool:
		return ( self.code[2:], self.code[0:2], self.region, self.name ) < \
			( entry.code[2:], entry.code[0:2], entry.region, entry.name )

	def requiresCartID(self) -> bool:
		if self.flags & DataFlag.DATA_HAS_CART_ID:
			return True
		if (self.flags & DataFlag.DATA_HAS_TRACE_ID) and \
			(self.traceIDType >= TraceIDType.TID_82_BIG_ENDIAN):
			return True

		return False

	def serialize(self) -> bytes:
		return _CART_DB_ENTRY_STRUCT.pack(
			self.chipType,
			self.formatType,
			self.traceIDType,
			self.flags,
			self.traceIDParam,
			self.installIDPrefix,
			self.year,
			self.dataKey,
			self.code.encode("ascii"),
			self.region.encode("ascii"),
			self.name.encode("ascii")
		)

@dataclass
class ROMHeaderDBEntry:
	code:   str
	region: str
	name:   str

	formatType: FormatType
	flags:      DataFlag

	year: int = 0

	def __init__(
		self, code: str, region: str, name: str, parser: ROMHeaderParser
	):
		self.code       = code
		self.region     = region
		self.name       = name
		self.formatType = parser.getFormatType()
		self.flags      = parser.flags
		self.year       = parser.year or 0

	def __lt__(self, entry: Any) -> bool:
		return ( self.code[2:], self.code[0:2], self.region, self.name ) < \
			( entry.code[2:], entry.code[0:2], entry.region, entry.name )

	def serialize(self) -> bytes:
		return _ROM_HEADER_DB_ENTRY_STRUCT.pack(
			self.formatType,
			self.flags,
			self.year,
			self.code.encode("ascii"),
			self.region.encode("ascii"),
			self.name.encode("ascii")
		)

## Data format identification

_KNOWN_CART_FORMATS: Sequence[tuple[str, Type, DataFlag]] = (
	(
		# Used by GCB48 (and possibly other games?)
		"region only",
		SimpleCartParser,
		DataFlag.DATA_HAS_PUBLIC_SECTION
	), (
		"basic (no IDs)",
		BasicCartParser,
		DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"basic + TID",
		BasicCartParser,
		DataFlag.DATA_HAS_TRACE_ID | DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"basic + SID",
		BasicCartParser,
		DataFlag.DATA_HAS_CART_ID | DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"basic + TID, SID",
		BasicCartParser,
		DataFlag.DATA_HAS_TRACE_ID | DataFlag.DATA_HAS_CART_ID
			| DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"basic + prefix, TID, SID",
		BasicCartParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_HAS_TRACE_ID
			| DataFlag.DATA_HAS_CART_ID | DataFlag.DATA_CHECKSUM_INVERTED
	), (
		# Used by most pre-ZS01 Bemani games
		"basic + prefix, all IDs",
		BasicCartParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_HAS_TRACE_ID
			| DataFlag.DATA_HAS_CART_ID | DataFlag.DATA_HAS_INSTALL_ID
			| DataFlag.DATA_HAS_SYSTEM_ID | DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"extended (no IDs)",
		ExtendedCartParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"extended (no IDs, alt)",
		ExtendedCartParser,
		DataFlag.DATA_HAS_CODE_PREFIX
	), (
		# Used by GX706
		"extended (no IDs, GX706)",
		ExtendedCartParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_GX706_WORKAROUND
	), (
		# Used by GE936/GK936 and all ZS01 Bemani games
		"extended + all IDs",
		ExtendedCartParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_HAS_TRACE_ID
			| DataFlag.DATA_HAS_CART_ID | DataFlag.DATA_HAS_INSTALL_ID
			| DataFlag.DATA_HAS_SYSTEM_ID | DataFlag.DATA_HAS_PUBLIC_SECTION
			| DataFlag.DATA_CHECKSUM_INVERTED
	)
)

_KNOWN_ROM_HEADER_FORMATS: Sequence[tuple[str, Type, DataFlag]] = (
	(
		"extended (no MD5)",
		ExtendedROMHeaderParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_CHECKSUM_INVERTED
	), (
		"extended (no MD5, alt)",
		ExtendedROMHeaderParser,
		DataFlag.DATA_HAS_CODE_PREFIX
	), (
		# Used by GX706
		"extended (no MD5, GX706)",
		ExtendedROMHeaderParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_GX706_WORKAROUND
	), (
		"extended + MD5",
		ExtendedROMHeaderParser,
		DataFlag.DATA_HAS_CODE_PREFIX | DataFlag.DATA_HAS_SYSTEM_ID
			 | DataFlag.DATA_CHECKSUM_INVERTED
	)
)

def newCartParser(dump: CartDump) -> CartParser:
	for name, constructor, flags in reversed(_KNOWN_CART_FORMATS):
		try:
			parser: Any = constructor(dump, flags)
		except ParserError as exc:
			logging.debug(f"parsing as {name} failed, {exc}")
			continue

		return parser

	raise RuntimeError("no known data format found")

def newROMHeaderParser(dump: ROMHeaderDump) -> ROMHeaderParser:
	for name, constructor, flags in reversed(_KNOWN_ROM_HEADER_FORMATS):
		try:
			parser: Any = constructor(dump, flags)
		except ParserError as exc:
			logging.debug(f"parsing as {name} failed, {exc}")
			continue

		return parser

	raise RuntimeError("no known data format found")
