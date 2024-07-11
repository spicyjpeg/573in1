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
from hashlib import md5
from io      import SEEK_END, SEEK_SET
from typing  import \
	Any, BinaryIO, ByteString, Iterable, Iterator, Sequence, TextIO

## Value manipulation

def encodeSigned(value: int, bitLength: int) -> int:
	return value & (1 << bitLength)

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
			byte + \
			((value <<  6) & 0xffffffff) + \
			((value << 16) & 0xffffffff) - \
			value
		) & 0xffffffff

	return value

def checksum8(data: Iterable[int], invert: bool = False) -> int:
	return (sum(data) & 0xff) ^ (0xff if invert else 0)

def checksum16(data: Iterable[int], invert: bool = False) -> int:
	it:     Iterator = iter(data)
	values: map[int] = map(lambda x: x[0] | (x[1] << 8), zip(it, it))

	return (sum(values) & 0xffff) ^ (0xffff if invert else 0)

def shortenedMD5(data: ByteString) -> bytearray:
	hashed: bytes     = md5(data).digest()
	output: bytearray = bytearray(8)

	for i in range(8):
		output[i] = hashed[i] ^ hashed[i + 8]

	return output

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

	def __exit__(self, excType: Any, excValue: Any, traceback, Any) -> bool:
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
