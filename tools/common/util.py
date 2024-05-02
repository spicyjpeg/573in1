# -*- coding: utf-8 -*-

import logging, re
from collections import deque
from hashlib     import md5
from io          import SEEK_END, SEEK_SET
from typing      import \
	BinaryIO, ByteString, Iterable, Iterator, Mapping, MutableSequence, \
	Sequence, TextIO

import numpy

## Value and array manipulation

def signExtend(value: int, bitLength: int) -> int:
	signMask:  int = 1 << (bitLength - 1)
	valueMask: int = signMask - 1

	return (value & valueMask) - (value & signMask)

def blitArray(
	source: numpy.ndarray, dest: numpy.ndarray, position: Sequence[int]
):
	pos: map[int | None] = map(lambda x: x if x >= 0 else None, position)
	neg: map[int | None] = map(lambda x: -x if x < 0 else None, position)

	destView:   numpy.ndarray = dest[tuple(
		slice(start, None) for start in pos
	)]
	sourceView: numpy.ndarray = source[tuple(
		slice(start, end) for start, end in zip(neg, destView.shape)
	)]

	destView[tuple(
		slice(None, end) for end in source.shape
	)] = sourceView

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

## Boolean algebra expression parser

class BooleanOperator:
	precedence: int = 1
	operands:   int = 2

	@staticmethod
	def execute(stack: MutableSequence[bool]):
		pass

class AndOperator(BooleanOperator):
	precedence: int = 2

	@staticmethod
	def execute(stack: MutableSequence[bool]):
		a: bool = stack.pop()
		b: bool = stack.pop()

		stack.append(a and b)

class OrOperator(BooleanOperator):
	@staticmethod
	def execute(stack: MutableSequence[bool]):
		a: bool = stack.pop()
		b: bool = stack.pop()

		stack.append(a or b)

class XorOperator(BooleanOperator):
	@staticmethod
	def execute(stack: MutableSequence[bool]):
		a: bool = stack.pop()
		b: bool = stack.pop()

		stack.append(a != b)

class NotOperator(BooleanOperator):
	precedence: int = 3
	operands:   int = 1

	@staticmethod
	def execute(stack: MutableSequence):
		stack.append(not stack.pop())

_OPERATORS: Mapping[str, type[BooleanOperator]] = {
	"*": AndOperator,
	"+": OrOperator,
	"@": XorOperator,
	"~": NotOperator
}

class BooleanFunction:
	def __init__(self, expression: str):
		# "Compile" the expression to its respective RPN representation using
		# the shunting yard algorithm.
		self.expression: list[str | type[BooleanOperator]] = []

		operators:   deque[str] = deque()
		tokenBuffer: str = ""

		for char in expression:
			if char not in "*+@~()":
				tokenBuffer += char
				continue

			# Flush the non-operator token buffer when an operator is
			# encountered.
			if tokenBuffer:
				self.expression.append(tokenBuffer)
				tokenBuffer = ""

			match char:
				case "(":
					operators.append(char)
				case ")":
					if "(" not in operators:
						raise RuntimeError("mismatched parentheses in expression")
					while (op := operators.pop()) != "(":
						self.expression.append(_OPERATORS[op])
				case _:
					precedence: int = _OPERATORS[char].precedence

					while operators:
						op: str = operators[-1]

						if op == "(":
							break
						if _OPERATORS[op].precedence < precedence:
							break

						self.expression.append(_OPERATORS[op])
						operators.pop()

					operators.append(char)

		if tokenBuffer:
			self.expression.append(tokenBuffer)
			tokenBuffer = ""

		if "(" in operators:
			raise RuntimeError("mismatched parentheses in expression")
		while operators:
			self.expression.append(_OPERATORS[operators.pop()])

	def evaluate(self, variables: Mapping[str, bool]) -> bool:
		values: dict[str, bool] = { "0": False, "1": True, **variables }
		stack:  deque[bool]     = deque()

		for token in self.expression:
			if isinstance(token, str):
				value: bool | None = values.get(token)

				if value is None:
					raise RuntimeError(f"unknown variable '{token}'")

				stack.append(value)
			else:
				token.execute(stack)

		if len(stack) != 1:
			raise RuntimeError("invalid or malformed expression")

		return stack[0]

## Logic lookup table conversion

def generateLUTFromExpression(expression: str, inputs: Sequence[str]) -> int:
	lut: int = 0

	function:  BooleanFunction = BooleanFunction(expression)
	variables: dict[str, bool] = {}

	for index in range(1 << len(inputs)):
		for bit, name in enumerate(inputs):
			variables[name] = bool((index >> bit) & 1)

		if function.evaluate(variables):
			lut |= 1 << index # LSB-first

	return lut

def generateExpressionFromLUT(lut: int, inputs: Sequence[str]) -> str:
	products: list[str] = []

	for index in range(1 << len(inputs)):
		values: str = "*".join(
			(value if (index >> bit) & 1 else f"~{value}")
			for bit, value in enumerate(inputs)
		)

		if (lut >> index) & 1:
			products.append(f"({values})")

	return "+".join(products) or "0"
