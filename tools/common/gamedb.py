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

from collections.abc import ByteString, Iterable, Mapping
from dataclasses     import dataclass
from enum            import IntEnum, IntFlag
from struct          import Struct
from typing          import Any, Self

from .util import JSONGroupedObject

## Utilities

def _toJSONObject(value: Any) -> Any:
	if hasattr(value, "toJSONObject"):
		value = value.toJSONObject()
	elif isinstance(value, ByteString):
		value = value.hex("-")
	elif isinstance(value, IntFlag):
		value = [ flag.name for flag in value ]

	if (value == 0) or (value == False) or (value == ""):
		return None
	else:
		return value

def _makeJSONObject(*groups: Mapping[str, Any]) -> JSONGroupedObject:
	jsonObj: JSONGroupedObject = JSONGroupedObject()

	for group in groups:
		dest: dict[str, Any] = {}

		for key, value in group.items():
			jsonValue: Any = _toJSONObject(value)

			if jsonValue is not None:
				dest[key] = jsonValue

		if dest:
			jsonObj.groups.append(dest)

	return jsonObj

def _groupJSONObject(obj: Any, *groups: Iterable[str]) -> JSONGroupedObject:
	jsonObj: JSONGroupedObject = JSONGroupedObject()

	for group in groups:
		dest: dict[str, Any] = {}

		for key in group:
			jsonValue: Any = _toJSONObject(getattr(obj, key, None))

			if jsonValue is not None:
				dest[key] = jsonValue

		if dest:
			jsonObj.groups.append(dest)

	return jsonObj

## Flags

class CartPCBType(IntEnum):
	CART_UNKNOWN_X76F041        =  1
	CART_UNKNOWN_X76F041_DS2401 =  2
	CART_UNKNOWN_ZS01           =  3
	CART_GX700_PWB_D            =  4
	CART_GX700_PWB_E            =  5
	CART_GX700_PWB_J            =  6
	CART_GX883_PWB_D            =  7
	CART_GX894_PWB_D            =  8
	CART_GX896_PWB_A_A          =  9
	CART_GE949_PWB_D_A          = 10
	CART_GE949_PWB_D_B          = 12
	CART_PWB0000068819          = 12
	CART_PWB0000088954          = 13

	@staticmethod
	def fromJSONObject(obj: str) -> Self:
		return {
			"unknown-x76f041":        CartPCBType.CART_UNKNOWN_X76F041,
			"unknown-x76f041-ds2401": CartPCBType.CART_UNKNOWN_X76F041_DS2401,
			"unknown-zs01":           CartPCBType.CART_UNKNOWN_ZS01,
			"GX700-PWB(D)":           CartPCBType.CART_GX700_PWB_D,
			"GX700-PWB(E)":           CartPCBType.CART_GX700_PWB_E,
			"GX700-PWB(J)":           CartPCBType.CART_GX700_PWB_J,
			"GX883-PWB(D)":           CartPCBType.CART_GX883_PWB_D,
			"GX894-PWB(D)":           CartPCBType.CART_GX894_PWB_D,
			"GX896-PWB(A)A":          CartPCBType.CART_GX896_PWB_A_A,
			"GE949-PWB(D)A":          CartPCBType.CART_GE949_PWB_D_A,
			"GE949-PWB(D)B":          CartPCBType.CART_GE949_PWB_D_B,
			"PWB0000068819":          CartPCBType.CART_PWB0000068819,
			"PWB0000088954":          CartPCBType.CART_PWB0000088954
		}[obj]

	def toJSONObject(self) -> str:
		return {
			CartPCBType.CART_UNKNOWN_X76F041:        "unknown-x76f041",
			CartPCBType.CART_UNKNOWN_X76F041_DS2401: "unknown-x76f041-ds2401",
			CartPCBType.CART_UNKNOWN_ZS01:           "unknown-zs01",
			CartPCBType.CART_GX700_PWB_D:            "GX700-PWB(D)",
			CartPCBType.CART_GX700_PWB_E:            "GX700-PWB(E)",
			CartPCBType.CART_GX700_PWB_J:            "GX700-PWB(J)",
			CartPCBType.CART_GX883_PWB_D:            "GX883-PWB(D)",
			CartPCBType.CART_GX894_PWB_D:            "GX894-PWB(D)",
			CartPCBType.CART_GX896_PWB_A_A:          "GX896-PWB(A)A",
			CartPCBType.CART_GE949_PWB_D_A:          "GE949-PWB(D)A",
			CartPCBType.CART_GE949_PWB_D_B:          "GE949-PWB(D)B",
			CartPCBType.CART_PWB0000068819:          "PWB0000068819",
			CartPCBType.CART_PWB0000088954:          "PWB0000088954"
		}[self]

class HeaderFlag(IntFlag):
	FORMAT_BITMASK        = 3 << 0
	FORMAT_SIMPLE         = 0 << 0
	FORMAT_BASIC          = 1 << 0
	FORMAT_EXTENDED       = 2 << 0
	SPEC_TYPE_BITMASK     = 3 << 2
	SPEC_TYPE_NONE        = 0 << 2
	SPEC_TYPE_ACTUAL      = 1 << 2
	SPEC_TYPE_WILDCARD    = 2 << 2
	HEADER_SCRAMBLED      = 1 << 4
	HEADER_IN_PUBLIC_AREA = 1 << 5
	REGION_LOWERCASE      = 1 << 6

	@staticmethod
	def fromJSONObject(obj: Mapping[str, Any]) -> Self:
		flags: HeaderFlag = 0

		flags |= {
			"simple":   HeaderFlag.FORMAT_SIMPLE,
			"basic":    HeaderFlag.FORMAT_BASIC,
			"extended": HeaderFlag.FORMAT_EXTENDED
		}[obj.get("format", None)]
		flags |= {
			None:       HeaderFlag.SPEC_TYPE_NONE,
			"actual":   HeaderFlag.SPEC_TYPE_ACTUAL,
			"wildcard": HeaderFlag.SPEC_TYPE_WILDCARD
		}[obj.get("specType", None)]

		for key, flag in {
			"scrambled":       HeaderFlag.HEADER_SCRAMBLED,
			"usesPublicArea":  HeaderFlag.HEADER_IN_PUBLIC_AREA,
			"lowercaseRegion": HeaderFlag.REGION_LOWERCASE
		}.items():
			if obj.get(key, False):
				flags |= flag

		return flags

	def toJSONObject(self) -> JSONGroupedObject:
		return _makeJSONObject(
			{
				"format": {
					HeaderFlag.FORMAT_SIMPLE:   "simple",
					HeaderFlag.FORMAT_BASIC:    "basic",
					HeaderFlag.FORMAT_EXTENDED: "extended"
				}[self & HeaderFlag.FORMAT_BITMASK],
				"specType": {
					HeaderFlag.SPEC_TYPE_NONE:     None,
					HeaderFlag.SPEC_TYPE_ACTUAL:   "actual",
					HeaderFlag.SPEC_TYPE_WILDCARD: "wildcard"
				}[self & HeaderFlag.SPEC_TYPE_BITMASK],

				"scrambled":       (HeaderFlag.HEADER_SCRAMBLED      in self),
				"usesPublicArea":  (HeaderFlag.HEADER_IN_PUBLIC_AREA in self),
				"lowercaseRegion": (HeaderFlag.REGION_LOWERCASE      in self)
			}
		)

class ChecksumFlag(IntFlag):
	CHECKSUM_UNIT_BITMASK     = 3 << 0
	CHECKSUM_UNIT_BYTE        = 0 << 0
	CHECKSUM_UNIT_WORD_LITTLE = 1 << 0
	CHECKSUM_UNIT_WORD_BIG    = 2 << 0
	CHECKSUM_BIG_ENDIAN       = 1 << 2
	CHECKSUM_INVERTED         = 1 << 3
	CHECKSUM_FORCE_GX_SPEC    = 1 << 4

	@staticmethod
	def fromJSONObject(obj: Mapping[str, Any]) -> Self:
		flags: ChecksumFlag = 0

		flags |= {
			"byte":             ChecksumFlag.CHECKSUM_UNIT_BYTE,
			"littleEndianWord": ChecksumFlag.CHECKSUM_UNIT_WORD_LITTLE,
			"bigEndianWord":    ChecksumFlag.CHECKSUM_UNIT_WORD_BIG
		}[obj.get("unit", None)]

		for key, flag in {
			"bigEndian":   ChecksumFlag.CHECKSUM_BIG_ENDIAN,
			"inverted":    ChecksumFlag.CHECKSUM_INVERTED,
			"forceGXSpec": ChecksumFlag.CHECKSUM_FORCE_GX_SPEC
		}.items():
			if obj.get(key, False):
				flags |= flag

		return flags

	def toJSONObject(self) -> JSONGroupedObject:
		return _makeJSONObject(
			{
				"unit": {
					ChecksumFlag.CHECKSUM_UNIT_BYTE:        "byte",
					ChecksumFlag.CHECKSUM_UNIT_WORD_LITTLE: "littleEndianWord",
					ChecksumFlag.CHECKSUM_UNIT_WORD_BIG:    "bigEndianWord"
				}[self & ChecksumFlag.CHECKSUM_UNIT_BITMASK],

				"bigEndian":   (ChecksumFlag.CHECKSUM_BIG_ENDIAN    in self),
				"inverted":    (ChecksumFlag.CHECKSUM_INVERTED      in self),
				"forceGXSpec": (ChecksumFlag.CHECKSUM_FORCE_GX_SPEC in self)
			}
		)

class IdentifierFlag(IntFlag):
	PRIVATE_TID_TYPE_BITMASK         = 3 << 0
	PRIVATE_TID_TYPE_NONE            = 0 << 0
	PRIVATE_TID_TYPE_STATIC          = 1 << 0
	PRIVATE_TID_TYPE_SID_HASH_LITTLE = 2 << 0
	PRIVATE_TID_TYPE_SID_HASH_BIG    = 3 << 0
	PRIVATE_SID_PRESENT              = 1 << 2
	PRIVATE_MID_PRESENT              = 1 << 3
	PRIVATE_XID_PRESENT              = 1 << 4
	ALLOCATE_DUMMY_PUBLIC_AREA       = 1 << 5
	PUBLIC_MID_PRESENT               = 1 << 6
	PUBLIC_XID_PRESENT               = 1 << 7

	@staticmethod
	def fromJSONObject(obj: Mapping[str, Any]) -> Self:
		flags: IdentifierFlag = 0

		flags |= {
			None:                  IdentifierFlag.PRIVATE_TID_TYPE_NONE,
			"static":              IdentifierFlag.PRIVATE_TID_TYPE_STATIC,
			"littleEndianSIDHash": IdentifierFlag.PRIVATE_TID_TYPE_SID_HASH_LITTLE,
			"bigEndianSIDHash":    IdentifierFlag.PRIVATE_TID_TYPE_SID_HASH_BIG
		}[obj.get("privateTID", None)]

		for key, flag in {
			"privateSID":      IdentifierFlag.PRIVATE_SID_PRESENT,
			"privateMID":      IdentifierFlag.PRIVATE_MID_PRESENT,
			"privateXID":      IdentifierFlag.PRIVATE_XID_PRESENT,
			"dummyPublicArea": IdentifierFlag.ALLOCATE_DUMMY_PUBLIC_AREA,
			"publicMID":       IdentifierFlag.PUBLIC_MID_PRESENT,
			"publicXID":       IdentifierFlag.PUBLIC_XID_PRESENT
		}.items():
			if obj.get(key, False):
				flags |= flag

		return flags

	def toJSONObject(self) -> JSONGroupedObject:
		return _makeJSONObject(
			{
				"privateTID": {
					IdentifierFlag.PRIVATE_TID_TYPE_NONE:            None,
					IdentifierFlag.PRIVATE_TID_TYPE_STATIC:          "static",
					IdentifierFlag.PRIVATE_TID_TYPE_SID_HASH_LITTLE: "littleEndianSIDHash",
					IdentifierFlag.PRIVATE_TID_TYPE_SID_HASH_BIG:    "bigEndianSIDHash"
				}[self & IdentifierFlag.PRIVATE_TID_TYPE_BITMASK],

				"privateSID": (IdentifierFlag.PRIVATE_SID_PRESENT in self),
				"privateMID": (IdentifierFlag.PRIVATE_MID_PRESENT in self),
				"privateXID": (IdentifierFlag.PRIVATE_XID_PRESENT in self)
			}, {
				"dummyPublicArea":
					(IdentifierFlag.ALLOCATE_DUMMY_PUBLIC_AREA in self),
				"publicMID": (IdentifierFlag.PUBLIC_MID_PRESENT in self),
				"publicXID": (IdentifierFlag.PUBLIC_XID_PRESENT in self)
			}
		)

class SignatureFlag(IntFlag):
	SIGNATURE_TYPE_BITMASK  = 3 << 0
	SIGNATURE_TYPE_NONE     = 0 << 0
	SIGNATURE_TYPE_CHECKSUM = 1 << 0
	SIGNATURE_TYPE_MD5      = 2 << 0
	SIGNATURE_DUMMY_01      = 1 << 2
	SIGNATURE_PAD_WITH_FF   = 1 << 3

	@staticmethod
	def fromJSONObject(obj: Mapping[str, Any]) -> Self:
		flags: SignatureFlag = 0

		flags |= {
			None:       SignatureFlag.SIGNATURE_TYPE_NONE,
			"checksum": SignatureFlag.SIGNATURE_TYPE_CHECKSUM,
			"md5":      SignatureFlag.SIGNATURE_TYPE_MD5
		}[obj.get("type", None)]

		for key, flag in {
			"dummy01":   SignatureFlag.SIGNATURE_DUMMY_01,
			"padWithFF": SignatureFlag.SIGNATURE_PAD_WITH_FF
		}.items():
			if obj.get(key, False):
				flags |= flag

		return flags

	def toJSONObject(self) -> JSONGroupedObject:
		return _makeJSONObject(
			{
				"type": {
					SignatureFlag.SIGNATURE_TYPE_NONE:     None,
					SignatureFlag.SIGNATURE_TYPE_CHECKSUM: "checksum",
					SignatureFlag.SIGNATURE_TYPE_MD5:      "md5"
				}[self & SignatureFlag.SIGNATURE_TYPE_BITMASK],

				"dummy01":   (SignatureFlag.SIGNATURE_DUMMY_01    in self),
				"padWithFF": (SignatureFlag.SIGNATURE_PAD_WITH_FF in self)
			}
		)

class GameFlag(IntFlag):
	GAME_IO_BOARD_BITMASK            = 7 << 0
	GAME_IO_BOARD_NONE               = 0 << 0
	GAME_IO_BOARD_ANALOG             = 1 << 0
	GAME_IO_BOARD_KICK               = 2 << 0
	GAME_IO_BOARD_FISHING_REEL       = 3 << 0
	GAME_IO_BOARD_DIGITAL            = 4 << 0
	GAME_IO_BOARD_DDR_KARAOKE        = 5 << 0
	GAME_IO_BOARD_GUNMANIA           = 6 << 0
	GAME_INSTALL_RTC_HEADER_REQUIRED = 1 << 3
	GAME_RTC_HEADER_REQUIRED         = 1 << 4

	@staticmethod
	def fromJSONObject(obj: Mapping[str, Any]) -> Self:
		flags: GameFlag = 0

		flags |= {
			None:            GameFlag.GAME_IO_BOARD_NONE,
			"GX700-PWB(F)":  GameFlag.GAME_IO_BOARD_ANALOG,
			"GX700-PWB(K)":  GameFlag.GAME_IO_BOARD_KICK,
			"GE765-PWB(B)A": GameFlag.GAME_IO_BOARD_FISHING_REEL,
			"GX894-PWB(B)A": GameFlag.GAME_IO_BOARD_DIGITAL,
			"GX921-PWB(B)":  GameFlag.GAME_IO_BOARD_DDR_KARAOKE,
			"PWB0000073070": GameFlag.GAME_IO_BOARD_GUNMANIA
		}[obj.get("ioBoard", None)]

		for key, flag in {
			"installRequiresRTCHeader": GameFlag.GAME_INSTALL_RTC_HEADER_REQUIRED,
			"requiresRTCHeader":        GameFlag.GAME_RTC_HEADER_REQUIRED
		}.items():
			if obj.get(key, False):
				flags |= flag

		return flags

	def toJSONObject(self) -> JSONGroupedObject:
		return _makeJSONObject(
			{
				"ioBoard": {
					GameFlag.GAME_IO_BOARD_NONE:         None,
					GameFlag.GAME_IO_BOARD_ANALOG:       "GX700-PWB(F)",
					GameFlag.GAME_IO_BOARD_KICK:         "GX700-PWB(K)",
					GameFlag.GAME_IO_BOARD_FISHING_REEL: "GE765-PWB(B)A",
					GameFlag.GAME_IO_BOARD_DIGITAL:      "GX894-PWB(B)A",
					GameFlag.GAME_IO_BOARD_DDR_KARAOKE:  "GX921-PWB(B)",
					GameFlag.GAME_IO_BOARD_GUNMANIA:     "PWB0000073070"
				}[self & GameFlag.GAME_IO_BOARD_BITMASK]
			}, {
				"installRequiresRTCHeader":
					(GameFlag.GAME_INSTALL_RTC_HEADER_REQUIRED in self),
				"requiresRTCHeader": (GameFlag.GAME_RTC_HEADER_REQUIRED in self)
			}
		)

## Data structures

_ROM_HEADER_INFO_STRUCT: Struct = Struct("< 2s 3B x")
_CART_INFO_STRUCT:       Struct = Struct("< 8s 2s 6B")
_GAME_INFO_STRUCT:       Struct = Struct("< 8s 36s 3s B 2H 6s 6s 16s 16s")
_MAX_SPECIFICATIONS:     int    = 4
_MAX_REGIONS:            int    = 12

@dataclass
class ROMHeaderInfo:
	yearField: bytes

	headerFlags:    HeaderFlag
	checksumFlags:  ChecksumFlag
	signatureFlags: SignatureFlag

	@staticmethod
	def fromJSONObject(obj: Mapping[str, Any]) -> Self:
		return ROMHeaderInfo(
			bytes.fromhex(obj["yearField"].replace("-", " ")),

			HeaderFlag   .fromJSONObject(obj.get("headerFlags",    {})),
			ChecksumFlag .fromJSONObject(obj.get("checksumFlags",  {})),
			SignatureFlag.fromJSONObject(obj.get("signatureFlags", {}))
		)

	def toJSONObject(self) -> JSONGroupedObject:
		return _groupJSONObject(
			self, (
				"yearField"
			), (
				"headerFlags",
				"checksumFlags",
				"signatureFlags"
			)
		)

	def toBinary(self) -> bytes:
		return _ROM_HEADER_INFO_STRUCT.pack(
			self.yearField,
			self.headerFlags,
			self.checksumFlags,
			self.signatureFlags
		)

@dataclass
class CartInfo:
	pcb: CartPCBType

	dataKey:   bytes
	yearField: bytes
	tidWidth:  int
	midValue:  int

	headerFlags:   HeaderFlag
	checksumFlags: ChecksumFlag
	idFlags:       IdentifierFlag

	@staticmethod
	def fromJSONObject(obj: Mapping[str, Any]) -> Self:
		return CartInfo(
			CartPCBType.fromJSONObject(obj["pcb"]),

			bytes.fromhex(obj["dataKey"]  .replace("-", " ")),
			bytes.fromhex(obj["yearField"].replace("-", " ")),
			int(obj.get("tidWidth", 0)),
			int(obj.get("midValue", 0)),

			HeaderFlag    .fromJSONObject(obj.get("headerFlags",   {})),
			ChecksumFlag  .fromJSONObject(obj.get("checksumFlags", {})),
			IdentifierFlag.fromJSONObject(obj.get("idFlags",       {}))
		)

	def toJSONObject(self) -> JSONGroupedObject:
		return _groupJSONObject(
			self, (
				"pcb",
			), (
				"dataKey",
				"yearField",
				"tidWidth",
				"midValue"
			), (
				"headerFlags",
				"checksumFlags",
				"idFlags",
				"tidWidth",
				"midValue"
			)
		)

	def toBinary(self) -> bytes:
		return _CART_INFO_STRUCT.pack(
			self.dataKey,
			self.yearField,
			self.pcb,
			self.tidWidth,
			self.midValue,
			self.headerFlags,
			self.checksumFlags,
			self.idFlags
		)

@dataclass
class GameInfo:
	specifications: list[str]
	code:           str
	regions:        list[str]
	identifiers:    list[str | None]

	name:   str
	series: str | None
	year:   int

	flags: GameFlag

	bootloaderVersion: str | None = None

	rtcHeader:   ROMHeaderInfo | None = None
	flashHeader: ROMHeaderInfo | None = None
	installCart: CartInfo      | None = None
	gameCart:    CartInfo      | None = None

	@staticmethod
	def fromJSONObject(obj: Mapping[str, Any]) -> Self:
		rtcHeader:   Mapping[str, Any] | None = obj.get("rtcHeader",   None)
		flashHeader: Mapping[str, Any] | None = obj.get("flashHeader", None)
		installCart: Mapping[str, Any] | None = obj.get("installCart", None)
		gameCart:    Mapping[str, Any] | None = obj.get("gameCart",    None)

		return GameInfo(
			obj["specifications"],
			obj["code"],
			obj["regions"],
			obj["identifiers"],

			obj["name"],
			obj.get("series", None),
			obj["year"],
			GameFlag.fromJSONObject(obj.get("flags", {})),

			obj.get("bootloaderVersion", None),

			ROMHeaderInfo.fromJSONObject(rtcHeader)   if rtcHeader   else None,
			ROMHeaderInfo.fromJSONObject(flashHeader) if flashHeader else None,
			CartInfo     .fromJSONObject(installCart) if installCart else None,
			CartInfo     .fromJSONObject(gameCart)    if gameCart    else None
		)

	def toJSONObject(self) -> JSONGroupedObject:
		return _groupJSONObject(
			self, (
				"specifications",
				"code",
				"regions",
				"identifiers"
			), (
				"name",
				"series",
				"year"
			), (
				"flags",
			), (
				"bootloaderVersion",
			), (
				"rtcHeader",
				"flashHeader",
				"installCart",
				"gameCart"
			)
		)

	def toBinary(self, nameOffset: int) -> bytes:
		if len(self.specifications) > _MAX_SPECIFICATIONS:
			raise ValueError(
				f"entry can only have up to {_MAX_SPECIFICATIONS} "
				f"specification codes"
			)
		if len(self.regions) > _MAX_REGIONS:
			raise ValueError(
				f"entry can only have up to {_MAX_REGIONS} region codes"
			)

		# FIXME: identifiers, series and bootloaderVersion are not currently
		# included in the binary format
		return _GAME_INFO_STRUCT.pack(
			b"".join(sorted(
				spec.encode("ascii").ljust(2, b"\0")
				for spec in self.specifications
			)),
			b"".join(sorted(
				region.encode("ascii").ljust(3, b"\0")
				for region in self.regions
			)),
			self.code.encode("ascii"),
			self.flags,
			nameOffset,
			self.year,
			self.rtcHeader  .toBinary(),
			self.flashHeader.toBinary(),
			self.installCart.toBinary(),
			self.gameCart   .toBinary()
		)
