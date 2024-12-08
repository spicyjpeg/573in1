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
from itertools       import product
from struct          import Struct
from typing          import ByteString

from .cart   import *
from .gamedb import *
from .util   import checksum8, checksum16, dsCRC8, sidCRC16

## Utilities

class ParserError(Exception):
	pass

def _unscrambleRTCRAM(data: ByteString) -> bytearray:
	# Some early games "scramble" RTC RAM by (possibly accidentally?)
	# interpreting the data to be written as an array of 16-bit big endian
	# values, then expanding them to 32-bit little endian.
	output: bytearray = bytearray(len(data) // 2)

	for i in range(0, len(output), 2):
		#if data[(i * 2) + 2] or data[(i * 2) + 3]:
			#raise ParserError("data does not seem to be scrambled")

		output[i + 0] = data[(i * 2) + 1]
		output[i + 1] = data[(i * 2) + 0]

	return output

def _validateCustomID(data: ByteString) -> bool:
	if not sum(data):
		return False

	checksum: int = checksum8(data[0:7], True)

	if checksum == data[7]:
		return True

	raise ParserError(
		f"checksum mismatch: expected {checksum:#04x}, got {data[7]:#04x}"
	)

def _validateDS2401ID(data: ByteString) -> bool:
	if not sum(data):
		return False
	if not data[0] or (data[0] == 0xff):
		raise ParserError(f"invalid 1-wire prefix {data[0]:#04x}")

	crc: int = dsCRC8(data[0:7])

	if crc == data[7]:
		return True

	raise ParserError(f"CRC8 mismatch: expected {crc:#04x}, got {data[7]:#04x}")

## Header checksum detection

def detectChecksum(data: ByteString, checksum: int) -> ChecksumFlag:
	buffer:       bytearray = bytearray(data)
	bigEndianSum: int       = (0
		| ((checksum << 8) & 0xff00)
		| ((checksum >> 8) & 0x00ff)
	)

	for unit, bigEndian, inverted, forceGXSpec in product(
		(
			ChecksumFlag.CHECKSUM_UNIT_BYTE,
			ChecksumFlag.CHECKSUM_UNIT_WORD_LITTLE,
			ChecksumFlag.CHECKSUM_UNIT_WORD_BIG
		),
		( 0, ChecksumFlag.CHECKSUM_BIG_ENDIAN ),
		( 0, ChecksumFlag.CHECKSUM_INVERTED ),
		( 0, ChecksumFlag.CHECKSUM_FORCE_GX_SPEC )
	):
		checksumFlags: ChecksumFlag = \
			ChecksumFlag(unit | bigEndian | inverted | forceGXSpec)
		flagList:      str          = \
			"|".join(flag.name for flag in checksumFlags) or "0"

		# Dark Horse Legend sets the game code to GE706, but mistakenly computes
		# the checksum as if the specification were GX.
		actual: int = bigEndianSum if bigEndian   else checksum
		buffer[0:2] = b"GX"        if forceGXSpec else data[0:2]

		match unit:
			case ChecksumFlag.CHECKSUM_UNIT_BYTE:
				expected: int = checksum8(buffer, bool(inverted))

			case ChecksumFlag.CHECKSUM_UNIT_WORD_LITTLE:
				expected: int = checksum16(buffer, "little", bool(inverted))

			case ChecksumFlag.CHECKSUM_UNIT_WORD_BIG:
				expected: int = checksum16(buffer, "big",    bool(inverted))

		if expected == actual:
			return checksumFlags
		else:
			logging.debug(
				f"    <{flagList}>: expected {expected:#06x}, got {actual:#06x}"
			)

	raise ParserError("could not find any valid header checksum format")

## Header format detection

# spec[0]:     always G
# spec[1]:     product type (B, C, E, K, L, N, Q, U, X, *=wildcard)
# code[0:2]:   game code (700-999 or A00-D99)
# region[0]:   region code
#              (A=Asia, E=Europe, J=Japan, K=Korea, S=Singapore?, U=US)
# region[1]:   major version code (A-F=regular, R-W=e-Amusement, X-Z=?)
# region[2:4]: minor version code (A-D or Z00-Z99, optional)
_SPECIFICATION_REGEX: re.Pattern = re.compile(rb"G[A-Z*]")
_CODE_REGEX:          re.Pattern = re.compile(rb"[0-9A-D][0-9][0-9]")
_REGION_REGEX:        re.Pattern = \
	re.compile(rb"[AEJKSU][A-FR-WX-Z]([A-D]|Z[0-9][0-9])?", re.IGNORECASE)

_BASIC_HEADER_STRUCT:    Struct = Struct("< 2s 2s B 3x")
_EXTENDED_HEADER_STRUCT: Struct = Struct("< 2s 6s 2s 4s H")
_PRIVATE_ID_STRUCT:      Struct = Struct("< 8s 8s 8s 8s")
_PUBLIC_ID_STRUCT:       Struct = Struct("< 8s 8s")

@dataclass
class DetectedHeader:
	yearField:     int          = 0
	headerFlags:   HeaderFlag   = HeaderFlag(0)
	checksumFlags: ChecksumFlag = ChecksumFlag(0)

	privateIDOffset: int | None = None
	publicIDOffset:  int | None = None

def detectHeader(
	data:          ByteString,
	privateOffset: int,
	publicOffset:  int
) -> DetectedHeader:
	unscrambledData: bytearray = _unscrambleRTCRAM(data)

	for formatType, scrambled, usesPublicArea in product(
		(
			HeaderFlag.FORMAT_SIMPLE,
			HeaderFlag.FORMAT_BASIC,
			HeaderFlag.FORMAT_EXTENDED
		),
		( HeaderFlag(0), HeaderFlag.HEADER_SCRAMBLED ),
		( HeaderFlag(0), HeaderFlag.HEADER_IN_PUBLIC_AREA )
	):
		header: DetectedHeader = DetectedHeader()

		header.headerFlags = formatType | scrambled | usesPublicArea
		flagList: str      = \
			"|".join(flag.name for flag in header.headerFlags) or "0"

		buffer: ByteString = unscrambledData if scrambled      else data
		offset: int | None = publicOffset    if usesPublicArea else privateOffset

		if (offset < 0) or (offset >= len(buffer)):
			logging.debug(f"  <{flagList}>: header offset out of bounds")
			continue

		match formatType:
			case HeaderFlag.FORMAT_SIMPLE:
				region:        bytes = buffer[offset:offset + 4]
				specification: bytes = b""

			case HeaderFlag.FORMAT_BASIC:
				region, specification, checksum = \
					_BASIC_HEADER_STRUCT.unpack_from(buffer, offset)

				header.privateIDOffset = offset + _BASIC_HEADER_STRUCT.size

				try:
					header.checksumFlags = \
						detectChecksum(buffer[offset:offset + 4], checksum)
				except ParserError as err:
					logging.debug(f"  <{flagList}>: {err}")
					continue

			case HeaderFlag.FORMAT_EXTENDED:
				(
					specification,
					code,
					header.yearField,
					region,
					checksum
				) = \
					_EXTENDED_HEADER_STRUCT.unpack_from(buffer, offset)

				header.publicIDOffset  = offset + _EXTENDED_HEADER_STRUCT.size
				header.privateIDOffset = \
					header.publicIDOffset + _PUBLIC_ID_STRUCT.size

				if (
					not _SPECIFICATION_REGEX.match(specification) or
					not _CODE_REGEX.match(code)
				):
					logging.debug(f"  <{flagList}>: invalid game code")
					continue

				try:
					header.checksumFlags = \
						detectChecksum(buffer[offset:offset + 14], checksum)
				except ParserError as err:
					logging.debug(f"  <{flagList}>: {err}")
					continue

		if not _REGION_REGEX.match(region):
			logging.debug(f"  <{flagList}>: invalid game region")
			continue

		if region == region.lower():
			header.headerFlags |= HeaderFlag.REGION_LOWERCASE

		if _SPECIFICATION_REGEX.match(specification):
			if specification[1] == "*":
				header.headerFlags |= HeaderFlag.SPEC_TYPE_WILDCARD
			else:
				header.headerFlags |= HeaderFlag.SPEC_TYPE_ACTUAL

		return header

	raise ParserError("could not find any valid header data format")

## Identifier detection

_TID_WIDTHS: Sequence[int] = 16, 14

@dataclass
class DetectedIdentifiers:
	tidWidth: int            = 0
	midValue: int            = 0
	idFlags:  IdentifierFlag = IdentifierFlag(0)

def detectPrivateIDs(
	data:            ByteString,
	privateOffset:   int,
	dummyAreaOffset: int
) -> DetectedIdentifiers:
	ids: DetectedIdentifiers = DetectedIdentifiers()

	# Dancing Stage EuroMIX uses an X76F041 cartridge but adopts the same data
	# layout as ZS01 games (32-byte public header/IDs + 32-byte private IDs).
	# However, as the X76F041 does not support leaving only the first 32 bytes
	# unprotected, the public area is instead relocated to the chip's last
	# 128-byte sector (which is then configured to be unprotected). This has to
	# be taken into account here as the private IDs are *not* moved to the
	# beginning of the first sector; the space that would otherwise .
	offset: int = privateOffset

	if (dummyAreaOffset >= 0) and (dummyAreaOffset < len(data)):
		dummyArea: ByteString = \
			data[dummyAreaOffset:dummyAreaOffset + _PRIVATE_ID_STRUCT.size]

		if sum(dummyArea):
			offset      = dummyAreaOffset
			ids.idFlags = IdentifierFlag.ALLOCATE_DUMMY_PUBLIC_AREA

	tid, sid, mid, xid = _PRIVATE_ID_STRUCT.unpack_from(data, offset)

	if _validateCustomID(tid):
		match tid[0]:
			case 0x81:
				ids.idFlags |= IdentifierFlag.PRIVATE_TID_TYPE_STATIC

			case 0x82:
				littleEndianCRC: int = int.from_bytes(tid[1:3], "little")
				bigEndianCRC:    int = int.from_bytes(tid[1:3], "big")

				for width in _TID_WIDTHS:
					crc: int = sidCRC16(sid[1:7], width)

					if crc == littleEndianCRC:
						ids.tidWidth = width
						ids.idFlags |= \
							IdentifierFlag.PRIVATE_TID_TYPE_SID_HASH_LITTLE
						break
					elif crc == bigEndianCRC:
						ids.tidWidth = width
						ids.idFlags |= \
							IdentifierFlag.PRIVATE_TID_TYPE_SID_HASH_BIG
						break

				raise ParserError("could not determine trace ID bit width")

			case _:
				raise ParserError(f"unknown trace ID prefix: {tid[0]:#04x}")

	if _validateDS2401ID(sid):
		ids.idFlags |= IdentifierFlag.PRIVATE_SID_PRESENT
	if _validateCustomID(mid):
		ids.midValue = mid[0]
		ids.idFlags |= IdentifierFlag.PRIVATE_MID_PRESENT
	if _validateDS2401ID(xid):
		ids.idFlags |= IdentifierFlag.PRIVATE_XID_PRESENT

	return ids

def detectPublicIDs(data: ByteString, publicOffset: int) -> DetectedIdentifiers:
	ids: DetectedIdentifiers = DetectedIdentifiers()

	mid, xid = _PUBLIC_ID_STRUCT.unpack_from(data, publicOffset)

	if _validateCustomID(mid):
		ids.midValue = mid[0]
		ids.idFlags |= IdentifierFlag.PUBLIC_MID_PRESENT
	if _validateDS2401ID(xid):
		ids.idFlags |= IdentifierFlag.PUBLIC_XID_PRESENT

	return ids

## Installation signature detection

_SIGNATURE_STRUCT: Struct = Struct("< 8s 8s")

def detectSignature(data: ByteString, publicOffset: int) -> SignatureFlag:
	signatureFlags: SignatureFlag = SignatureFlag(0)

	installSig, dummy = _SIGNATURE_STRUCT.unpack_from(data, publicOffset)

	# TODO: implement

	return signatureFlags

## Parsing API

def parseCartHeader(dump: CartDump, pcb: CartPCBType | None = None) -> CartInfo:
	if pcb is None:
		match dump.chipType, bool(dump.flags & DumpFlag.DUMP_HAS_CART_ID):
			case ChipType.X76F041, False:
				pcb = CartPCBType.CART_UNKNOWN_X76F041

			case ChipType.X76F041, True:
				pcb = CartPCBType.CART_UNKNOWN_X76F041_DS2401

			case ChipType.ZS01,    True:
				pcb = CartPCBType.CART_UNKNOWN_ZS01

			case _, _:
				raise ParserError("unsupported cartridge type")

	chipSize: ChipSize       = dump.getChipSize()
	header:   DetectedHeader = detectHeader(
		dump.data,
		chipSize.privateDataOffset,
		chipSize.publicDataOffset
	)

	if header.privateIDOffset is None:
		privateIDs: DetectedIdentifiers = DetectedIdentifiers()
	else:
		privateIDs: DetectedIdentifiers = detectPrivateIDs(
			dump.data,
			header.privateIDOffset,
			header.privateIDOffset
				- chipSize.publicDataOffset
				+ chipSize.privateDataOffset
		)

	if header.publicIDOffset is None:
		publicIDs: DetectedIdentifiers = DetectedIdentifiers()
	else:
		publicIDs: DetectedIdentifiers = \
			detectPublicIDs(dump.data, header.publicIDOffset)

	if (
		(IdentifierFlag.PRIVATE_MID_PRESENT in privateIDs.idFlags) and
		(IdentifierFlag.PUBLIC_MID_PRESENT  in publicIDs.idFlags)
	):
		if privateIDs.midValue != publicIDs.midValue:
			raise ParserError("private and public MID values do not match")

	return CartInfo(
		pcb,
		dump.dataKey,
		header.yearField,
		privateIDs.tidWidth,
		privateIDs.midValue,
		header.headerFlags,
		header.checksumFlags,
		privateIDs.idFlags | publicIDs.idFlags
	)

def parseROMHeader(dump: ROMHeaderDump) -> ROMHeaderInfo:
	header: DetectedHeader = detectHeader(dump.data, -1, FLASH_HEADER_OFFSET)

	if header.publicIDOffset is None:
		signatureFlags: SignatureFlag = SignatureFlag(0)
	else:
		signatureFlags: SignatureFlag = \
			detectSignature(dump.data, header.publicIDOffset)

	return ROMHeaderInfo(
		header.yearField,
		header.headerFlags,
		header.checksumFlags,
		signatureFlags
	)
