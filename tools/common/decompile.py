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

from struct import Struct
from typing import Any, BinaryIO, ByteString, Generator, TextIO

from .mips import Instruction, Opcode, Register, encodeJR, parseInstruction

## Executable analyzer

def parseStructFromFile(file: BinaryIO, _struct: Struct) -> tuple:
	return _struct.unpack(file.read(_struct.size))

_EXE_HEADER_STRUCT: Struct = Struct("< 8s 8x 4I 16x 2I 20x 1972s")
_EXE_HEADER_MAGIC:  bytes  = b"PS-X EXE"

class PSEXEAnalyzer:
	def __init__(self, file: BinaryIO):
		(
			magic,
			entryPoint,
			initialGP,
			startAddress,
			length,
			stackOffset,
			stackLength,
			_
		) = \
			parseStructFromFile(file, _EXE_HEADER_STRUCT)

		if magic != _EXE_HEADER_MAGIC:
			raise RuntimeError("file is not a valid PS1 executable")

		self.entryPoint:   int   = entryPoint
		self.startAddress: int   = startAddress
		self.endAddress:   int   = startAddress + length
		self.body:         bytes = file.read(length)

		#file.close()

	def __getitem__(self, key: int | slice) -> Any:
		if isinstance(key, slice):
			return self.body[self._makeSlice(key.start, key.stop, key.step)]
		else:
			return self.body[key - self.startAddress]

	def _makeSlice(
		self, start: int | None = None, stop: int | None = None, step: int = 1
	) -> slice:
		_start: int = \
			0              if (start is None) else (start - self.startAddress)
		_stop:  int = \
			len(self.body) if (stop  is None) else (stop  - self.startAddress)

		# Allow for searching/disassembling backwards by swapping the start and
		# stop parameters.
		if _start > _stop:
			#_start -= step
			#_stop  -= step
			step = -step

		return slice(_start, _stop, step)

	def disassembleAt(self, address: int) -> Instruction | None:
		offset: int = address - self.startAddress

		try:
			return parseInstruction(address, self.body[offset:offset + 4])
		except:
			return None

	def disassemble(
		self, start: int | None = None, stop: int | None = None
	) -> Generator[Instruction | None, None, None]:
		area:   slice = self._makeSlice(start, stop, 4)
		offset: int   = area.start

		if (area.start % 4) or (area.stop % 4):
			raise ValueError("unaligned start and/or end addresses")

		while offset != area.stop:
			address: int = self.startAddress + offset

			try:
				yield parseInstruction(address, self.body[offset:offset + 4])
			except:
				yield None

			offset += area.step

	def dumpDisassembly(
		self, output: TextIO, start: int | None = None, stop: int | None = None
	):
		for inst in self.disassemble(start, stop):
			if inst is not None:
				output.write(f"{inst.address:08x}:   {inst.toString()}\n")

	def findBytes(
		self, data: ByteString, start: int | None = None,
		stop: int | None = None, alignment: int = 4
	) -> Generator[int, None, None]:
		area:   slice = self._makeSlice(start, stop)
		offset: int   = area.start

		if area.step > 0:
			step:  int      = len(data)
			index: function = \
				lambda offset: self.body.index(data, offset, area.stop)
		else:
			step:  int      = -len(data)
			index: function = \
				lambda offset: self.body.rindex(data, area.stop, offset)

		while True:
			try:
				offset = index(offset)
			except ValueError:
				return

			if not (offset % alignment):
				yield self.startAddress + offset

			offset += step

	def findFunctionReturns(
		self, start: int | None = None, stop: int | None = None
	) -> Generator[int, None, None]:
		inst: bytes = encodeJR(Register.RA)

		for offset in self.findBytes(inst, start, stop, 4):
			yield offset + 8

	def findCalls(
		self, address: int, start: int | None = None, stop: int | None = None
	) -> Generator[Instruction, None, None]:
		for inst in self.disassemble(start, stop):
			if inst is None:
				continue
			if inst.opcode != Opcode.JAL:
				continue
			if inst.target != address:
				continue

			yield inst
