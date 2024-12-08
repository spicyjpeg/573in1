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

import json, logging, re
from collections.abc import \
	ByteString, Generator, Iterable, Iterator, Mapping, Sequence
from dataclasses     import dataclass, field
from functools       import reduce
from hashlib         import md5
from itertools       import chain
from io              import SEEK_END, SEEK_SET
from typing          import Any, BinaryIO, TextIO

## Value manipulation

def roundUpToMultiple(value: int, length: int) -> int:
	diff: int = value % length

	return (value - diff + length) if diff else value

def encodeSigned(value: int, bitLength: int) -> int:
	valueMask: int = (1 << bitLength) - 1

	return value & valueMask

def decodeSigned(value: int, bitLength: int) -> int:
	signMask:  int = 1 << (bitLength - 1)
	valueMask: int = signMask - 1

	return (value & valueMask) - (value & signMask)

## String manipulation

# This encoding is similar to standard base45, but with some problematic
# characters (' ', '$', '%', '*') excluded.
_BASE41_CHARSET: str = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./:"

_COLOR_REGEX: re.Pattern = re.compile(r"^#?([0-9A-Fa-f]{3}|[0-9A-Fa-f]{6})$")

def toPrintableChar(value: int) -> str:
	if (value < 0x20) or (value > 0x7e):
		return "."

	return chr(value)

def hexdumpToFile(data: Sequence[int], output: TextIO, width: int = 16):
	for i in range(0, len(data), width):
		hexBytes: map[str] = map(lambda value: f"{value:02x}", data[i:i + width])
		hexLine:  str      = " ".join(hexBytes).ljust(width * 3 - 1)

		asciiBytes: map[str] = map(toPrintableChar, data[i:i + width])
		asciiLine:  str      = "".join(asciiBytes).ljust(width)

		output.write(f"  {i:04x}: {hexLine} |{asciiLine}|\n")

def serialNumberToString(_id: ByteString) -> str:
	value: int = int.from_bytes(_id[1:7], "little")

	#if value >= 100000000:
		#return "xxxx-xxxx"

	return f"{(value // 10000) % 10000:04d}-{value % 10000:04d}"

def decodeBase41(data: str) -> bytearray:
	mapped: map[int]  = map(_BASE41_CHARSET.index, data)
	output: bytearray = bytearray()

	for a, b, c in zip(mapped, mapped, mapped):
		value: int = a + (b * 41) + (c * 1681)

		output.append(value >> 8)
		output.append(value & 0xff)

	return output

def colorFromString(value: str) -> tuple[int, int, int]:
	matched: re.Match | None = _COLOR_REGEX.match(value)

	if matched is None:
		raise ValueError(f"invalid color value '{value}'")

	digits: str = matched.group(1)

	if len(digits) == 3:
		return (
			int(digits[0], 16) * 0x11,
			int(digits[1], 16) * 0x11,
			int(digits[2], 16) * 0x11
		)
	else:
		return (
			int(digits[0:2], 16),
			int(digits[2:4], 16),
			int(digits[4:6], 16)
		)

## Hashes and checksums

def hashData(data: Iterable[int]) -> int:
	value: int = 0

	for byte in data:
		value = (
			byte +
			((value <<  6) & 0xffffffff) +
			((value << 16) & 0xffffffff) -
			value
		) & 0xffffffff

	return value

def checksum8(data: Iterable[int], invert: bool = False) -> int:
	return (sum(data) & 0xff) ^ (0xff if invert else 0)

def checksum16(
	data: Iterable[int], endianness: str = "little", invert: bool = False
) -> int:
	it:     Iterator = iter(data)
	values: map[int] = map(lambda x: int.from_bytes(x, endianness), zip(it, it))

	return (sum(values) & 0xffff) ^ (0xffff if invert else 0)

def shortenedMD5(data: ByteString) -> bytearray:
	hashed: bytes     = md5(data).digest()
	output: bytearray = bytearray(8)

	for i in range(8):
		output[i] = hashed[i] ^ hashed[i + 8]

	return output

## CRC calculation

_CRC8_POLY: int = 0x8c

def dsCRC8(data: ByteString) -> int:
	crc: int = 0

	for byte in data:
		for _ in range(8):
			temp: int = crc ^ byte

			byte >>= 1
			crc  >>= 1

			if temp & 1:
				crc ^= _CRC8_POLY

	return crc & 0xff

def sidCRC16(data: ByteString, width: int = 16) -> int:
	crc: int = 0

	for i, byte in enumerate(data):
		for j in range(i * 8, (i + 1) * 8):
			if byte & 1:
				crc ^= 1 << (j % width)

			byte >>= 1

	return crc & 0xffff

## Logging

def setupLogger(level: int | None):
	logging.basicConfig(
		format = "[{levelname:8s}] {message}",
		style  = "{",
		level  = (
			logging.WARNING,
			logging.INFO,
			logging.DEBUG
		)[min(level or 0, 2)]
	)

## JSON pretty printing

@dataclass
class JSONGroupedArray:
	groups: list[Sequence] = field(default_factory = list)

	def merge(self) -> list:
		return list(chain(*self.groups))

@dataclass
class JSONGroupedObject:
	groups: list[Mapping] = field(default_factory = list)

	def merge(self) -> Mapping:
		return reduce(lambda a, b: a | b, self.groups)

class JSONFormatter:
	def __init__(
		self,
		minify:                bool = False,
		groupedOnSingleLine:   bool = False,
		ungroupedOnSingleLine: bool = True,
		indentString:          str  = "\t"
	):
		self.minify:                bool = minify
		self.groupedOnSingleLine:   bool = groupedOnSingleLine
		self.ungroupedOnSingleLine: bool = ungroupedOnSingleLine
		self.indentString:          str  = indentString

		self._indentLevel:     int = 0
		self._forceSingleLine: int = 0

	def _inlineSep(self, char: str) -> str:
		if self.minify:
			return char
		elif char in ")]}":
			return f" {char}"
		else:
			return f"{char} "

	def _lineBreak(self, numBreaks: int = 1) -> str:
		if self.minify:
			return ""
		else:
			return ("\n" * numBreaks) + (self.indentString * self._indentLevel)

	def _singleLineArray(self, obj: Sequence) -> Generator[str, None, None]:
		if not obj:
			yield "[]"
			return

		self._forceSingleLine += 1
		yield self._inlineSep("[")

		lastIndex: int = len(obj) - 1

		for index, item in enumerate(obj):
			yield from self.serialize(item)

			if index < lastIndex:
				yield self._inlineSep(",")

		self._forceSingleLine -= 1
		yield self._inlineSep("]")

	def _singleLineObject(self, obj: Mapping) -> Generator[str, None, None]:
		if not obj:
			yield "{}"
			return

		self._forceSingleLine += 1
		yield self._inlineSep("{")

		lastIndex: int = len(obj) - 1

		for index, ( key, value ) in enumerate(obj.items()):
			yield from self.serialize(key)
			yield self._inlineSep(":")
			yield from self.serialize(value)

			if index < lastIndex:
				yield self._inlineSep(",")

		self._forceSingleLine -= 1
		yield self._inlineSep("}")

	def _groupedArray(
		self, groups: Sequence[Sequence]
	) -> Generator[str, None, None]:
		if not groups:
			yield "[]"
			return

		self._indentLevel += 1
		yield "[" + self._lineBreak()

		lastGroupIndex: int = len(groups) - 1

		for groupIndex, obj in enumerate(groups):
			if not obj:
				raise ValueError("empty groups are not allowed")

			lastIndex: int = len(obj) - 1

			for index, item in enumerate(obj):
				yield from self.serialize(item)

				if index < lastIndex:
					yield "," + self._lineBreak()

			if groupIndex < lastGroupIndex:
				yield "," + self._lineBreak(2)

		self._indentLevel -= 1
		yield self._lineBreak() + "]"

	def _groupedObject(
		self, groups: Sequence[Mapping]
	) -> Generator[str, None, None]:
		if not groups:
			yield "{}"
			return

		self._indentLevel += 1
		yield "{" + self._lineBreak()

		lastGroupIndex: int = len(groups) - 1

		for groupIndex, obj in enumerate(groups):
			if not obj:
				raise ValueError("empty groups are not allowed")

			keys: list[str] = [
				("".join(self.serialize(key)) + self._inlineSep(":"))
				for key in obj.keys()
			]

			lastIndex:    int = len(obj) - 1
			maxKeyLength: int = 0 if self.minify else max(map(len, keys))

			for index, value in enumerate(obj.values()):
				yield keys[index].ljust(maxKeyLength)
				yield from self.serialize(value)

				if index < lastIndex:
					yield "," + self._lineBreak()

			if groupIndex < lastGroupIndex:
				yield "," + self._lineBreak(2)

		self._indentLevel -= 1
		yield self._lineBreak() + "}"

	def serialize(self, obj: Any) -> Generator[str, None, None]:
		groupedOnSingleLine:   bool = \
			self.groupedOnSingleLine   or bool(self._forceSingleLine)
		ungroupedOnSingleLine: bool = \
			self.ungroupedOnSingleLine or bool(self._forceSingleLine)

		match obj:
			case JSONGroupedArray() if groupedOnSingleLine:
				yield from self._singleLineArray(obj.merge())
			case JSONGroupedArray() if not groupedOnSingleLine:
				yield from self._groupedArray(obj.groups)

			case JSONGroupedObject() if groupedOnSingleLine:
				yield from self._singleLineObject(obj.merge())
			case JSONGroupedObject() if not groupedOnSingleLine:
				yield from self._groupedObject(obj.groups)

			case (list() | tuple()) if ungroupedOnSingleLine:
				yield from self._singleLineArray(obj)
			case (list() | tuple()) if not ungroupedOnSingleLine:
				yield from self._groupedArray(( obj, ))

			case Mapping() if ungroupedOnSingleLine:
				yield from self._singleLineObject(obj)
			case Mapping() if not ungroupedOnSingleLine:
				yield from self._groupedObject(( obj, ))

			case _:
				yield json.dumps(obj, ensure_ascii = False)

## Hash table generator

@dataclass
class HashTableEntry:
	fullHash:   int
	chainIndex: int
	data:       Any

class HashTableBuilder:
	def __init__(self, numBuckets: int = 256):
		self._numBuckets: int = numBuckets

		self.entries: list[HashTableEntry | None] = [ None ] * numBuckets

	def addEntry(self, fullHash: int, data: Any) -> int:
		index: int = fullHash % self._numBuckets

		entry:  HashTableEntry        = HashTableEntry(fullHash, 0, data)
		bucket: HashTableEntry | None = self.entries[index]

		# If no bucket exists for the entry's index, create one.
		if bucket is None:
			self.entries[index] = entry
			return index
		if bucket.fullHash == fullHash:
			raise KeyError(f"hash collision detected ({fullHash:#010x})")

		# Otherwise, follow the buckets's chain, find the last chained item and
		# link the new entry to it.
		while bucket.chainIndex:
			bucket = self.entries[bucket.chainIndex]

			if bucket.fullHash == fullHash:
				raise KeyError(f"hash collision detected, ({fullHash:#010x})")

		bucket.chainIndex = len(self.entries)
		self.entries.append(entry)

		return bucket.chainIndex

class StringBlobBuilder:
	def __init__(self, alignment: int = 1):
		self._alignment: int                   = alignment
		self._offsets:   dict[ByteString, int] = {}

		self.data: bytearray = bytearray()

	def addString(self, string: ByteString) -> int:
		# If the same string is already in the blob, return its offset without
		# adding new data.
		offset: int | None = self._offsets.get(string, None)

		if offset is None:
			offset = len(self.data)

			self._offsets[string] = offset
			self.data            += string

			while len(self.data) % self._alignment:
				self.data.append(0)

		return offset

## Odd/even interleaved file reader

class InterleavedFile(BinaryIO):
	def __init__(self, even: BinaryIO, odd: BinaryIO):
		self._even:   BinaryIO = even
		self._odd:    BinaryIO = odd
		self._offset: int      = 0

		# Determine the total size of the file ahead of time.
		even.seek(0, SEEK_END)
		odd.seek(0, SEEK_END)

		self._length: int = even.tell()

		if self._length != odd.tell():
			raise RuntimeError("even and odd files must have the same size")

		even.seek(0, SEEK_SET)
		odd.seek(0, SEEK_SET)

	def __enter__(self) -> BinaryIO:
		return self

	def __exit__(self, excType: Any, excValue: Any, traceback: Any) -> bool:
		self.close()
		return False

	def close(self):
		self._even.close()
		self._odd.close()

	def seek(self, offset: int, mode: int = SEEK_SET):
		match mode:
			case 0:
				self._offset = offset
			case 1:
				self._offset = min(self._offset + offset, self._length)
			case 2:
				self._offset = max(self._length - offset, 0)

		self._even.seek((self._offset + 1) // 2)
		self._odd.seek(self._offset // 2)

	def tell(self) -> int:
		return self._offset

	def read(self, length: int) -> bytearray:
		_length: int       = min(length, self._length - self._offset)
		output:  bytearray = bytearray(_length)

		if self._offset % 2:
			output[0:_length:2] = self._odd.read((_length + 1) // 2)
			output[1:_length:2] = self._even.read(_length // 2)
		else:
			output[0:_length:2] = self._even.read((_length + 1) // 2)
			output[1:_length:2] = self._odd.read(_length // 2)

		self._offset += _length
		return output
