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

from collections.abc import ByteString, Generator
from struct          import Struct
from typing          import Any, BinaryIO, TextIO

from .mips import \
	ImmInstruction, Instruction, Opcode, Register, encodeJAL, encodeJR, \
	parseInstruction

## Executable analyzer

_EXE_HEADER_STRUCT: Struct = Struct("< 8s 8x 4I 16x 2I 20x 1972s")
_EXE_HEADER_MAGIC:  bytes  = b"PS-X EXE"

class AnalysisError(Exception):
	pass

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
			region
		) = \
			_EXE_HEADER_STRUCT.unpack(file.read(_EXE_HEADER_STRUCT.size))

		if magic != _EXE_HEADER_MAGIC:
			raise AnalysisError("file is not a valid PS1 executable")

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
		self,
		start: int | None = None,
		stop:  int | None = None,
		step:  int        = 1
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
		self,
		start: int | None = None,
		stop:  int | None = None
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
		self,
		output: TextIO,
		start:  int | None = None,
		stop:   int | None = None
	):
		for inst in self.disassemble(start, stop):
			if inst is not None:
				output.write(f"{inst.address:08x}:   {inst.toString()}\n")

	def findBytes(
		self,
		data:      ByteString,
		start:     int | None = None,
		stop:      int | None = None,
		alignment: int        = 4
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

	def findSingleMatch(
		self,
		data:      ByteString,
		start:     int | None = None,
		stop:      int | None = None,
		alignment: int        = 4
	) -> int:
		matches: Generator[int, None, None] = \
			self.findBytes(data, start, stop, alignment)

		try:
			firstMatch: int = next(matches)
		except StopIteration:
			raise AnalysisError("no match found")

		try:
			next(matches)
			raise AnalysisError("more than one match found")
		except StopIteration:
			return firstMatch

	def findFunctionReturns(
		self,
		start: int | None = None,
		stop:  int | None = None
	) -> Generator[int, None, None]:
		inst: bytes = encodeJR(Register.RA)

		# Yield pointers to the end of the return "statement", skipping the
		# instruction itself as well as its delay slot. In most cases these will
		# be pointers to the end of a function and thus the beginning of another
		# one.
		for offset in self.findBytes(inst, start, stop, 4):
			yield offset + 8

	def findCalls(
		self,
		target: int,
		start:  int | None = None,
		stop:   int | None = None
	) -> Generator[int, None, None]:
		inst: bytes = encodeJAL(target)

		yield from self.findBytes(inst, start, stop, 4)

	def findValueLoads(
		self,
		value:       int,
		start:       int | None = None,
		stop:        int | None = None,
		maxDistance: int        = 1
	) -> Generator[ImmInstruction, None, None]:
		# 32-bit loads are typically encoded as a LUI followed by either ORI or
		# ADDIU. Due to ADDIU only supporting signed immediates, the LUI's
		# immediate may not actually match the upper 16 bits of the value if the
		# ADDIU is supposed to subtract from it.
		for inst in self.disassemble(start, stop):
			if inst is None:
				continue

			for offset in range(4, (maxDistance + 1) * 4, 4):
				nextInst: Instruction | None = \
					self.disassembleAt(inst.address + offset)

				match inst, nextInst:
					case (
						ImmInstruction(
							opcode = Opcode.LUI, rt = rt, value = msb
						),
						ImmInstruction(
							opcode = Opcode.ORI, rs = rs, value = lsb
						)
					) if (rt == rs) and (((msb << 16) | lsb) == value):
						yield nextInst

					case (
						ImmInstruction(
							opcode = Opcode.LUI,   rt = rt, value = msb
						),
						ImmInstruction(
							opcode = Opcode.ADDIU, rs = rs, value = lsb
						)
					) if (rt == rs) and (((msb << 16) + lsb) == value):
						yield nextInst
