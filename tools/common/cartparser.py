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

from .cart   import *
from .gamedb import *
from .util   import \
	byteSwap, checksum8, checksum8to16, checksum16, dsCRC8, sidCRC16

## Utilities

class ParserError(Exception):
	pass

def _unscrambleRTCRAM(data: bytes | bytearray) -> bytearray:
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

def _isEmpty(data: bytes | bytearray) -> bool:
	for byte in data:
		if byte and (byte != 0xff):
			return False

	return True

def _validateCustomID(data: bytes | bytearray) -> bool:
	if not sum(data):
		return False

	checksum: int = checksum8(data[0:7], True)

	if checksum == data[7]:
		return True

	raise ParserError(
		f"checksum mismatch: expected {checksum:#04x}, got {data[7]:#04x}"
	)

def _validateDS2401ID(data: bytes | bytearray) -> bool:
	if not sum(data):
		return False
	if not data[0] or (data[0] == 0xff):
		raise ParserError(f"invalid 1-wire prefix {data[0]:#04x}")

	crc: int = dsCRC8(data[0:7])

	if crc == data[7]:
		return True

	raise ParserError(f"CRC8 mismatch: expected {crc:#04x}, got {data[7]:#04x}")

## Header checksum detection

def detectChecksum(data: bytes | bytearray, checksum: int) -> ChecksumFlag:
	# Steering/Handle Champ has no checksum in its flash header, despite
	# otherwise having one in RTC RAM.
	if not checksum:
		return ChecksumFlag.CHECKSUM_WIDTH_NONE

	buffer:      bytearray = bytearray(data)
	altChecksum: int       = byteSwap(checksum, 2)

	for (
		width,
		bigEndianInput,
		bigEndianOutput,
		inverted,
		forceGXSpec
	) in product(
		(
			ChecksumFlag.CHECKSUM_WIDTH_16,
			ChecksumFlag.CHECKSUM_WIDTH_8_IN_16_OUT,
			ChecksumFlag.CHECKSUM_WIDTH_8
		),
		( 0, ChecksumFlag.CHECKSUM_INPUT_BIG_ENDIAN ),
		( 0, ChecksumFlag.CHECKSUM_OUTPUT_BIG_ENDIAN ),
		( 0, ChecksumFlag.CHECKSUM_INVERTED ),
		( 0, ChecksumFlag.CHECKSUM_FORCE_GX_SPEC )
	):
		checksumFlags: ChecksumFlag = ChecksumFlag(0
			| width
			| bigEndianInput
			| bigEndianOutput
			| inverted
			| forceGXSpec
		)
		flagList: str = "|".join(flag.name for flag in checksumFlags) or "0"

		# Dark Horse Legend sets the game code to GE706 but mistakenly computes
		# the checksum as if the specification were GX.
		actual: int = altChecksum if bigEndianOutput else checksum
		buffer[0:2] = b"GX"       if forceGXSpec     else data[0:2]

		match width:
			case ChecksumFlag.CHECKSUM_WIDTH_8:
				expected: int = checksum8(buffer, bool(inverted))

			case ChecksumFlag.CHECKSUM_WIDTH_8_IN_16_OUT:
				expected: int = checksum8to16(buffer, bool(inverted))

			case ChecksumFlag.CHECKSUM_WIDTH_16:
				expected: int = checksum16(
					buffer,
					"big" if bigEndianInput else "little",
					bool(inverted)
				)

		logging.debug(
			f"    <{flagList}>: expected {expected:#06x}, got {actual:#06x}"
		)

		if expected == actual:
			return checksumFlags

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

_BASIC_HEADER_STRUCT:          Struct = Struct("< 2s 2s B 3x")
_EARLY_EXTENDED_HEADER_STRUCT: Struct = Struct("< 2s 6s 2s 6s")
_EXTENDED_HEADER_STRUCT:       Struct = Struct("< 2s 6s 2s 4s H")
_PRIVATE_ID_STRUCT:            Struct = Struct("< 8s 8s 8s 8s")
_PUBLIC_ID_STRUCT:             Struct = Struct("< 8s 8s")

@dataclass
class DetectedHeader:
	yearField:     bytes        = b""
	headerFlags:   HeaderFlag   = HeaderFlag(0)
	checksumFlags: ChecksumFlag = ChecksumFlag(0)

	privateIDOffset: int | None = None
	publicIDOffset:  int | None = None

def detectHeader(
	data:          bytes | bytearray,
	privateOffset: int,
	publicOffset:  int
) -> DetectedHeader:
	# The baseball games leave the 32-byte header area in the flash "empty"
	# (filled with a random looking sequence of 0x00 and 0xff bytes).
	if _isEmpty(data):
		return DetectedHeader(b"", HeaderFlag.FORMAT_NONE)

	unscrambledData: bytearray = _unscrambleRTCRAM(data)

	for formatType, scrambled, usesPublicArea in product(
		(
			HeaderFlag.FORMAT_EXTENDED,
			HeaderFlag.FORMAT_EARLY_EXTENDED,
			HeaderFlag.FORMAT_BASIC,
			HeaderFlag.FORMAT_REGION_ONLY
		),
		( HeaderFlag(0), HeaderFlag.HEADER_SCRAMBLED ),
		( HeaderFlag(0), HeaderFlag.HEADER_IN_PUBLIC_AREA )
	):
		header: DetectedHeader = DetectedHeader()

		header.headerFlags = formatType | scrambled | usesPublicArea
		flagList: str      = \
			"|".join(flag.name for flag in header.headerFlags) or "0"

		buffer: bytes | bytearray = unscrambledData if scrambled else data
		offset: int | None        = \
			publicOffset if usesPublicArea else privateOffset

		if (offset < 0) or (offset >= len(buffer)):
			logging.debug(f"  <{flagList}>: header offset out of bounds")
			continue

		match formatType:
			case HeaderFlag.FORMAT_REGION_ONLY:
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

			case HeaderFlag.FORMAT_EARLY_EXTENDED:
				# Early variant of the "extended" format used only by DDR JAB.
				# All strings are padded with spaces in place of null bytes, the
				# year field is ASCII rather than BCD and there is no checksum.
				(
					specification,
					code,
					header.yearField,
					region
				) = \
					_EARLY_EXTENDED_HEADER_STRUCT.unpack_from(buffer, offset)

				code:   bytes = code  .rstrip(b" ")
				region: bytes = region.rstrip(b" ")

				header.publicIDOffset  = offset + _EXTENDED_HEADER_STRUCT.size
				header.privateIDOffset = \
					header.publicIDOffset + _PUBLIC_ID_STRUCT.size

				if (
					not _SPECIFICATION_REGEX.match(specification) or
					not _CODE_REGEX.match(code)
				):
					logging.debug(f"  <{flagList}>: invalid game code")
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

		logging.debug(f"  <{flagList}>: header valid")
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
	data:            bytes | bytearray,
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
	# beginning of the first sector; the space that would otherwise be taken up
	# by the header and public IDs is still allocated and left blank.
	offset: int = privateOffset

	if (dummyAreaOffset >= 0) and (dummyAreaOffset < len(data)):
		dummyArea: bytes | bytearray = \
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
				actualLE: int = int.from_bytes(tid[1:3], "little")
				actualBE: int = int.from_bytes(tid[1:3], "big")

				for width in _TID_WIDTHS:
					expected: int = sidCRC16(sid[1:7], width)

					if expected == actualLE:
						ids.tidWidth = width
						ids.idFlags |= \
							IdentifierFlag.PRIVATE_TID_TYPE_SID_HASH_LE
						break
					elif expected == actualBE:
						ids.tidWidth = width
						ids.idFlags |= \
							IdentifierFlag.PRIVATE_TID_TYPE_SID_HASH_BE
						break

				if not ids.tidWidth:
					raise ParserError("could not determine TID bit width")

			case _:
				raise ParserError(f"unknown TID prefix: {tid[0]:#04x}")

	if _validateDS2401ID(sid):
		ids.idFlags |= IdentifierFlag.PRIVATE_SID_PRESENT
	if _validateCustomID(mid):
		ids.midValue = mid[0]
		ids.idFlags |= IdentifierFlag.PRIVATE_MID_PRESENT
	if _validateDS2401ID(xid):
		ids.idFlags |= IdentifierFlag.PRIVATE_XID_PRESENT

	return ids

def detectPublicIDs(
	data:         bytes | bytearray,
	publicOffset: int
) -> DetectedIdentifiers:
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

@dataclass
class DetectedSignature:
	signatureField: bytes         = b""
	signatureFlags: SignatureFlag = SignatureFlag(0)

def detectSignature(
	data:         bytes | bytearray,
	publicOffset: int
) -> DetectedSignature:
	sig: DetectedSignature = DetectedSignature()

	installSig, padding = _SIGNATURE_STRUCT.unpack_from(data, publicOffset)

	if installSig != padding:
		if sum(installSig[4:8]):
			# Assume any random-looking signature is MD5. This is not foolproof,
			# but the chance of an MD5 signature ending with four null bytes is
			# practically zero.
			sig.signatureFlags |= SignatureFlag.SIGNATURE_TYPE_MD5
		elif False:
			# DrumMania 1st-3rdMIX, GuitarFreaks 4thMIX, DS feat. TKD use a
			# different algorithm to derive the signature. PunchMania, GunMania
			# and DDR Karaoke Mix may also share the same algorithm.
			# TODO: reverse engineer the algorithm
			sig.signatureFlags |= SignatureFlag.SIGNATURE_TYPE_CHECKSUM
		else:
			# Salary Man Champ and Step Champ use a fixed signature baked into
			# the flash image on the installation disc.
			sig.signatureField  = installSig[0:4]
			sig.signatureFlags |= SignatureFlag.SIGNATURE_TYPE_STATIC

	paddingSum: int = sum(padding)

	if paddingSum == (0xff * len(padding)):
		sig.signatureFlags |= SignatureFlag.SIGNATURE_PAD_WITH_FF
	elif paddingSum:
		raise ParserError("signature area is padded with neither 0x00 nor 0xff")

	return sig

## Parsing API

def parseCartHeader(dump: CartDump, pcb: CartPCBType | None = None) -> CartInfo:
	if pcb is None:
		match dump.chipType, bool(dump.flags & DumpFlag.DUMP_HAS_CART_ID):
			case ChipType.X76F041, False:
				pcb = CartPCBType.CART_UNKNOWN_X76F041

			case ChipType.X76F041, True:
				pcb = CartPCBType.CART_UNKNOWN_X76F041_DS2401

			#case ChipType.ZS01, True:
			case ChipType.ZS01, _:
				pcb = CartPCBType.CART_UNKNOWN_ZS01

			case _, _:
				raise ParserError(
					f"{dump.chipType.name} cartridges are not supported"
				)

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

def parseROMHeader(
	dump: ROMHeaderDump, ignoreSignature: bool = False
) -> ROMHeaderInfo:
	header: DetectedHeader = detectHeader(dump.data, -1, FLASH_HEADER_OFFSET)

	if ignoreSignature or (header.publicIDOffset is None):
		sig: DetectedSignature = DetectedSignature()
	else:
		sig: DetectedSignature = \
			detectSignature(dump.data, header.publicIDOffset)

	# FIXME: games that use the scrambled RTC RAM data format require additional
	# data past the header in order to boot, unknown how to generate it
	return ROMHeaderInfo(
		sig.signatureField,
		header.yearField,
		header.headerFlags,
		header.checksumFlags,
		sig.signatureFlags
	)
