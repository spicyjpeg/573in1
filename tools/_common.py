# -*- coding: utf-8 -*-

__version__ = "0.3.1"
__author__  = "spicyjpeg"

import re
from dataclasses import dataclass
from enum        import IntEnum, IntFlag
from struct      import Struct
from typing      import Any, Iterable, Iterator, Mapping, Sequence

## Definitions

class ChipType(IntEnum):
	NONE    = 0
	X76F041 = 1
	X76F100 = 2
	ZS01    = 3

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

class DumpFlag(IntFlag):
	DUMP_HAS_SYSTEM_ID   = 1 << 0
	DUMP_HAS_CART_ID     = 1 << 1
	DUMP_CONFIG_OK       = 1 << 2
	DUMP_SYSTEM_ID_OK    = 1 << 3
	DUMP_CART_ID_OK      = 1 << 4
	DUMP_ZS_ID_OK        = 1 << 5
	DUMP_PUBLIC_DATA_OK  = 1 << 6
	DUMP_PRIVATE_DATA_OK = 1 << 7

class DataFlag(IntFlag):
	DATA_HAS_CODE_PREFIX    = 1 << 0
	DATA_HAS_TRACE_ID       = 1 << 1
	DATA_HAS_CART_ID        = 1 << 2
	DATA_HAS_INSTALL_ID     = 1 << 3
	DATA_HAS_SYSTEM_ID      = 1 << 4
	DATA_HAS_PUBLIC_SECTION = 1 << 5
	DATA_CHECKSUM_INVERTED  = 1 << 6
	DATA_GX706_WORKAROUND   = 1 << 7

# Character 0:    always G
# Character 1:    region related? (can be B, C, E, K, L, N, Q, U, X)
# Characters 2-4: identifier (700-999 or A00-A99 ~ D00-D99)
GAME_CODE_REGEX: re.Pattern = \
	re.compile(rb"G[A-Z][0-9A-D][0-9][0-9]", re.IGNORECASE)

# Character 0:    region (A=Asia?, E=Europe, J=Japan, K=Korea, S=?, U=US)
# Character 1:    type/variant (A-F=regular, R-W=e-Amusement, X-Z=?)
# Characters 2-4: game revision (A-D or Z00-Z99, optional)
GAME_REGION_REGEX: re.Pattern = \
	re.compile(rb"[AEJKSU][A-FR-WX-Z]([A-D]|Z[0-9][0-9])?", re.IGNORECASE)

SYSTEM_ID_IO_BOARDS: Sequence[str] = (
	"GX700-PWB(K)", # Kick & Kick expansion board
	"GX894-PWB(B)", # Digital I/O board
	"GX921-PWB(B)", # DDR Karaoke Mix expansion board
	"PWB0000073070" # GunMania expansion board
)

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

		self.traceID, self.cartID, self.installID, self.systemID = ids

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

	def getTraceIDType(self) -> TraceIDType:
		if self.traceID is None:
			return TraceIDType.TID_NONE

		match self.traceID[0]:
			case 0x81:
				return TraceIDType.TID_81

			case 0x82:
				return TraceIDType.TID_82_BIG_ENDIAN # TODO

			case prefix:
				raise ValueError(f"unknown trace ID prefix: 0x{prefix:02x}")

## Cartridge dump structure

_DUMP_HEADER_STRUCT: Struct = Struct("< 2B 2x 8s 8s 8s 8s 8s")

_CHIP_SIZES: Mapping[ChipType, tuple[int, int, int]] = {
	ChipType.X76F041: ( 512, 384, 128 ),
	ChipType.X76F100: ( 112,   0,   0 ),
	ChipType.ZS01:    ( 112,   0,  32 )
}

@dataclass
class Dump:
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
		return _DUMP_HEADER_STRUCT.pack(
			self.chipType,
			self.flags,
			self.systemID,
			self.cartID,
			self.zsID,
			self.dataKey,
			self.config
		) + self.data

def parseDump(data: bytes) -> Dump:
	chipType, flags, systemID, cartID, zsID, dataKey, config = \
		_DUMP_HEADER_STRUCT.unpack(data[0:_DUMP_HEADER_STRUCT.size])
	dataLength, _, _ = _CHIP_SIZES[chipType]

	return Dump(
		chipType, flags, systemID, cartID, zsID, dataKey, config,
		data[_DUMP_HEADER_STRUCT.size:_DUMP_HEADER_STRUCT.size + dataLength]
	)

## Cartridge data parsers

_BASIC_HEADER_STRUCT:    Struct = Struct("< 2s 2s B 3x")
_EXTENDED_HEADER_STRUCT: Struct = Struct("< 8s H 4s H")

# The system and install IDs are excluded from validation as they may not be
# always present.
_IDENTIFIER_FLAG_MASK: DataFlag = \
	DataFlag.DATA_HAS_TRACE_ID | DataFlag.DATA_HAS_CART_ID

def _checksum8(data: Iterable[int], invert: bool = False):
	return (sum(data) & 0xff) ^ (0xff if invert else 0)

def _checksum16(data: Iterable[int], invert: bool = False):
	it:     Iterator = iter(data)
	values: map[int] = map(lambda x: x[0] | (x[1] << 8), zip(it, it))

	return (sum(values) & 0xffff) ^ (0xffff if invert else 0)

def _getPublicData(dump: Dump, flags: DataFlag, maxLength: int = 512) -> bytes:
	if flags & DataFlag.DATA_HAS_PUBLIC_SECTION:
		_, offset, length = dump.getChipSize()

		return dump.data[offset:offset + min(length, maxLength)]
	else:
		return dump.data[0:maxLength]

class ParserError(BaseException):
	pass

@dataclass
class Parser:
	formatType:  FormatType
	flags:       DataFlag
	identifiers: IdentifierSet

	region:     str | None = None
	codePrefix: str | None = None
	code:       str | None = None
	year:       int | None = None

class SimpleParser(Parser):
	def __init__(self, dump: Dump, flags: DataFlag):
		region: bytes = _getPublicData(dump, flags, 8).rstrip(b"\0")

		if GAME_REGION_REGEX.fullmatch(region) is None:
			raise ParserError(f"invalid game region: {region}")

		super().__init__(
			FormatType.SIMPLE, flags, IdentifierSet(b""), region.decode("ascii")
		)

class BasicParser(Parser):
	def __init__(self, dump: Dump, flags: DataFlag):
		data: bytes = _getPublicData(dump, flags, _BASIC_HEADER_STRUCT.size)
		ids: IdentifierSet = IdentifierSet(dump.data[_BASIC_HEADER_STRUCT.size:])

		region, codePrefix, checksum = _BASIC_HEADER_STRUCT.unpack(data)

		codePrefix: bytes = codePrefix.rstrip(b"\0")
		value:      int   = _checksum8(
			data[0:4], bool(flags & DataFlag.DATA_CHECKSUM_INVERTED)
		)

		if value != checksum:
			raise ParserError(f"invalid header checksum, exp=0x{value:02x}, got=0x{checksum:02x}")
		if GAME_REGION_REGEX.fullmatch(region) is None:
			raise ParserError(f"invalid game region: {region}")
		if bool(flags & DataFlag.DATA_HAS_CODE_PREFIX) != bool(codePrefix):
			raise ParserError(f"game code prefix should{' not' if codePrefix else ''} be present")
		if (ids.getFlags() ^ flags) & _IDENTIFIER_FLAG_MASK:
			raise ParserError("identifier flags do not match")

		super().__init__(
			FormatType.BASIC, flags, ids, region.decode("ascii"),
			codePrefix.decode("ascii") or None
		)

class ExtendedParser(Parser):
	def __init__(self, dump: Dump, flags: DataFlag):
		data: bytes = _getPublicData(dump, flags, _EXTENDED_HEADER_STRUCT.size)
		ids:  IdentifierSet = IdentifierSet(dump.data[_EXTENDED_HEADER_STRUCT.size + 16:])

		if flags & DataFlag.DATA_GX706_WORKAROUND:
			data = data[0:1] + b"X" + data[2:]

		code, year, region, checksum = _EXTENDED_HEADER_STRUCT.unpack(data)

		code:   bytes = code.rstrip(b"\0")
		region: bytes = region.rstrip(b"\0")
		value:  int   = _checksum16(
			data[0:14], bool(flags & DataFlag.DATA_CHECKSUM_INVERTED)
		)

		if value != checksum:
			raise ParserError(f"invalid header checksum, exp=0x{value:04x}, got=0x{checksum:04x}")
		if GAME_CODE_REGEX.fullmatch(code) is None:
			raise ParserError(f"invalid game code: {code}")
		if GAME_REGION_REGEX.fullmatch(region) is None:
			raise ParserError(f"invalid game region: {region}")
		if (ids.getFlags() ^ flags) & _IDENTIFIER_FLAG_MASK:
			raise ParserError("identifier flags do not match")

		_code: str = code.decode("ascii")
		super().__init__(
			FormatType.EXTENDED, flags, ids, region.decode("ascii"), _code[0:2],
			_code, year
		)

## Cartridge database

DB_ENTRY_STRUCT: Struct = Struct("< 6B H 8s 8s 8s 96s")

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

@dataclass
class DBEntry:
	game:    GameEntry
	dataKey: bytes

	chipType:    ChipType
	formatType:  FormatType
	traceIDType: TraceIDType
	flags:       DataFlag

	traceIDParam:    int = 0
	installIDPrefix: int = 0
	year:            int = 0

	def __init__(self, game: GameEntry, dump: Dump, parser: Parser):
		self.game        = game
		self.dataKey     = dump.dataKey
		self.chipType    = dump.chipType
		self.formatType  = parser.formatType
		self.traceIDType = parser.identifiers.getTraceIDType()
		self.flags       = parser.flags
		self.year        = parser.year or 0

		# TODO: implement this properly
		self.traceIDParam = 16

		if parser.identifiers.installID:
			self.installIDPrefix = parser.identifiers.installID[0]
		else:
			self.installIDPrefix = 0

	def __lt__(self, entry: Any) -> bool:
		return (self.game < entry.game)

	def serialize(self) -> bytes:
		return DB_ENTRY_STRUCT.pack(
			self.chipType,
			self.formatType,
			self.traceIDType,
			self.flags,
			self.traceIDParam,
			self.installIDPrefix,
			self.year,
			self.dataKey,
			self.game.code.encode("ascii"),
			self.game.region.encode("ascii"),
			self.game.name.encode("ascii")
		)
