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

from dataclasses import dataclass
from enum        import IntEnum
from struct      import Struct
from typing      import Any, BinaryIO, ByteString, Generator, TextIO

from .util import decodeSigned, encodeSigned

## Register definitions

class Register(IntEnum):
	ZERO =  0
	AT   =  1
	V0   =  2
	V1   =  3
	A0   =  4
	A1   =  5
	A2   =  6
	A3   =  7
	T0   =  8
	T1   =  9
	T2   = 10
	T3   = 11
	T4   = 12
	T5   = 13
	T6   = 14
	T7   = 15
	S0   = 16
	S1   = 17
	S2   = 18
	S3   = 19
	S4   = 20
	S5   = 21
	S6   = 22
	S7   = 23
	T8   = 24
	T9   = 25
	K0   = 26
	K1   = 27
	GP   = 28
	SP   = 29
	FP   = 30
	RA   = 31

class COP0DataRegister(IntEnum):
	BPC      =  3 # Breakpoint program counter
	BDA      =  5 # Breakpoint data address
	DCIC     =  7 # Breakpoint control
	BADVADDR =  8 # Bad virtual address
	BDAM     =  9 # Breakpoint program counter bitmask
	BPCM     = 11 # Breakpoint data address bitmask
	SR       = 12 # Status register
	CAUSE    = 13 # Exception cause
	EPC      = 14 # Exception program counter
	PRID     = 15 # Processor identifier

class GTEControlRegister(IntEnum):
	RT11RT12 =  0 # Rotation matrix
	RT13RT21 =  1 # Rotation matrix
	RT22RT23 =  2 # Rotation matrix
	RT31RT32 =  3 # Rotation matrix
	RT33     =  4 # Rotation matrix
	TRX      =  5 # Translation vector
	TRY      =  6 # Translation vector
	TRZ      =  7 # Translation vector
	L11L12   =  8 # Light matrix
	L13L21   =  9 # Light matrix
	L22L23   = 10 # Light matrix
	L31L32   = 11 # Light matrix
	L33      = 12 # Light matrix
	RBK      = 13 # Background color
	GBK      = 14 # Background color
	BBK      = 15 # Background color
	LC11LC12 = 16 # Light color matrix
	LC13LC21 = 17 # Light color matrix
	LC22LC23 = 18 # Light color matrix
	LC31LC32 = 19 # Light color matrix
	LC33     = 20 # Light color matrix
	RFC      = 21 # Far color
	GFC      = 22 # Far color
	BFC      = 23 # Far color
	OFX      = 24 # Screen coordinate offset
	OFY      = 25 # Screen coordinate offset
	H        = 26 # Projection plane distance
	DQA      = 27 # Depth cue scale factor
	DQB      = 28 # Depth cue base
	ZSF3     = 29 # Average Z scale factor
	ZSF4     = 30 # Average Z scale factor
	FLAG     = 31 # Error/overflow flags

class GTEDataRegister(IntEnum):
	VXY0 =  0 # Input vector 0
	VZ0  =  1 # Input vector 0
	VXY1 =  2 # Input vector 1
	VZ1  =  3 # Input vector 1
	VXY2 =  4 # Input vector 2
	VZ2  =  5 # Input vector 2
	RGBC =  6 # Input color and GPU command
	OTZ  =  7 # Average Z value output
	IR0  =  8 # Scalar accumulator
	IR1  =  9 # Vector accumulator
	IR2  = 10 # Vector accumulator
	IR3  = 11 # Vector accumulator
	SXY0 = 12 # X/Y coordinate output FIFO
	SXY1 = 13 # X/Y coordinate output FIFO
	SXY2 = 14 # X/Y coordinate output FIFO
	SXYP = 15 # X/Y coordinate output FIFO
	SZ0  = 16 # Z coordinate output FIFO
	SZ1  = 17 # Z coordinate output FIFO
	SZ2  = 18 # Z coordinate output FIFO
	SZ3  = 19 # Z coordinate output FIFO
	RGB0 = 20 # Color and GPU command output FIFO
	RGB1 = 21 # Color and GPU command output FIFO
	RGB2 = 22 # Color and GPU command output FIFO
	MAC0 = 24 # Extended scalar accumulator
	MAC1 = 25 # Extended vector accumulator
	MAC2 = 26 # Extended vector accumulator
	MAC3 = 27 # Extended vector accumulator
	IRGB = 28 # RGB conversion input
	ORGB = 29 # RGB conversion output
	LZCS = 30 # Leading zero count input
	LZCR = 31 # Leading zero count output

## Opcode definitions

class Opcode(IntEnum):
	SUB_INST   =  0
	SUB_BRANCH =  1
	J          =  2 # Jump to immediate
	JAL        =  3 # Jump and link to immediate
	BEQ        =  4 # Branch if equal
	BNE        =  5 # Branch if not equal
	BLEZ       =  6 # Branch if less than or equal to zero
	BGTZ       =  7 # Branch if greater than zero
	ADDI       =  8 # Add immediate signed
	ADDIU      =  9 # Add immediate unsigned
	SLTI       = 10 # Set if less than immediate signed
	SLTIU      = 11 # Set if less than immediate unsigned
	ANDI       = 12 # Bitwise AND immediate
	ORI        = 13 # Bitwise OR immediate
	XORI       = 14 # Bitwise XOR immediate
	LUI        = 15 # Load upper immediate
	COP0       = 16 # COP0 instruction
	COP2       = 18 # GTE instruction
	LB         = 32 # Load byte signed
	LH         = 33 # Load halfword signed
	LWL        = 34 # Load word left
	LW         = 35 # Load word
	LBU        = 36 # Load byte unsigned
	LHU        = 37 # Store halfword unsigned
	LWR        = 38 # Load word right
	SB         = 40 # Store byte signed
	SH         = 41 # Store halfword signed
	SWL        = 42 # Store word left
	SW         = 43 # Store word
	SWR        = 46 # Store word right
	LWC2       = 50 # Load data word to GTE
	SWC2       = 58 # Store data word from GTE

class SubOpcode(IntEnum):
	SLL     =  0 # Shift left immediate
	SRL     =  2 # Shift right logical immediate
	SRA     =  3 # Shift right arithmetic immediate
	SLLV    =  4 # Shift left register
	SRLV    =  6 # Shift right logical register
	SRAV    =  7 # Shift right arithmetic register
	JR      =  8 # Jump to register
	JALR    =  9 # Jump and link to register
	SYSCALL = 12 # System call
	BREAK   = 13 # Breakpoint
	MFHI    = 16 # Move from $hi
	MTHI    = 17 # Move to $hi
	MFLO    = 18 # Move from $lo
	MTLO    = 19 # Move to $lo
	MULT    = 24 # Multiply signed
	MULTU   = 25 # Multiply unsigned
	DIV     = 26 # Divide signed
	DIVU    = 27 # Divide unsigned
	ADD     = 32 # Add register
	ADDU    = 33 # Add register unsigned
	SUB     = 34 # Subtract register
	SUBU    = 35 # Subtract register unsigned
	AND     = 36 # Bitwise AND register
	OR      = 37 # Bitwise OR register
	XOR     = 38 # Bitwise XOR register
	NOR     = 39 # Bitwise NOR register
	SLT     = 42 # Set if less than signed
	SLTU    = 43 # Set if less than unsigned

class SubBranchOpcode(IntEnum):
	BLTZ   =  0 # Branch if less than zero
	BGEZ   =  1 # Branch if greater than or equal to zero
	BLTZAL = 16 # Branch and link if less than zero
	BGEZAL = 17 # Branch and link if greater than or equal to zero

class COPMoveOpcode(IntEnum):
	MFC = 0 # Move data from coprocessor
	CFC = 2 # Move control from coprocessor
	MTC = 4 # Move data to coprocessor
	CTC = 6 # Move control to coprocessor

class COP0Opcode(IntEnum):
	RFE = 16 # Return from exception

class GTEOpcode(IntEnum):
	RTPS  =  1 # Perspective transformation (1 vertex)
	NCLIP =  6 # Normal clipping
	OP    = 12 # Outer product
	DPCS  = 16 # Depth cue (1 vertex)
	INTPL = 17 # Depth cue with vector
	MVMVA = 18 # Matrix-vector multiplication
	NCDS  = 19 # Normal color depth (1 vertex)
	CDP   = 20 # Color depth cue
	NCDT  = 22 # Normal color depth (3 vertices)
	NCCS  = 27 # Normal color color (1 vertex)
	CC    = 28 # Color color
	NCS   = 30 # Normal color (1 vertex)
	NCT   = 32 # Normal color (3 vertices)
	SQR   = 40 # Square of vector
	DCPL  = 41 # Depth cue with light
	DPCT  = 42 # Depth cue (3 vertices)
	AVSZ3 = 45 # Average Z value (3 vertices)
	AVSZ4 = 46 # Average Z value (4 vertices)
	RTPT  = 48 # Perspective transformation (3 vertices)
	GPF   = 61 # Linear interpolation
	GPL   = 62 # Linear interpolation with base
	NCCT  = 63 # Normal color color (3 vertices)

## Base instruction classes

@dataclass
class Instruction:
	address: int
	opcode:  Opcode

	def toInt(self) -> int:
		return (self.opcode << 26)

	def toBytes(self) -> bytes:
		return self.toInt().to_bytes(4, "little")

	def toString(self) -> str:
		return self.opcode.name.lower()

@dataclass
class SubInstruction(Instruction):
	sub: SubOpcode

	def toInt(self) -> int:
		return super().toInt() | self.sub

	def toString(self) -> str:
		return self.sub.name.lower()

## Instruction classes

@dataclass
class RegInstruction(SubInstruction):
	value: int
	rd:    Register = Register.ZERO
	rt:    Register = Register.ZERO
	rs:    Register = Register.ZERO

	def toInt(self) -> int:
		return (super().toInt()
			| (self.value <<  6)
			| (self.rd    << 11)
			| (self.rt    << 16)
			| (self.rs    << 21)
		)

	def toString(self) -> str:
		match self.sub:
			case SubOpcode.SLL | SubOpcode.SRL | SubOpcode.SRA:
				args: str = f"${self.rd.name}, ${self.rt.name}, {self.value}"

			case SubOpcode.SLLV | SubOpcode.SRLV | SubOpcode.SRAV:
				args: str = f"${self.rd.name}, ${self.rt.name}, ${self.rs.name}"

			case SubOpcode.JR | SubOpcode.MTHI | SubOpcode.MTLO:
				args: str = f"${self.rs.name}"

			case SubOpcode.JALR:
				if self.rd == Register.RA:
					args: str = f"${self.rs.name}"
				else:
					args: str = f"${self.rs.name}, ${self.rd.name}"

			case SubOpcode.MFHI | SubOpcode.MFLO:
				args: str = f"${self.rd.name}"

			case SubOpcode.MULT | SubOpcode.MULTU | SubOpcode.DIV | SubOpcode.DIVU:
				args: str = f"${self.rs.name}, ${self.rt.name}"

			case _:
				args: str = f"${self.rd.name}, ${self.rs.name}, ${self.rt.name}"

		return f"{super().toString():7s} {args}".lower()

@dataclass
class SysInstruction(SubInstruction):
	value: int

	def toInt(self) -> int:
		return (super().toInt() | (self.value << 6))

	def toString(self) -> str:
		return f"{super().toString():7s} {self.value:#x}"

@dataclass
class SubBranchInstruction(Instruction):
	target: int
	branch: SubBranchOpcode
	rs:     Register

	def toInt(self) -> int:
		value: int = encodeSigned(self.target // 4, 16)

		return (super().toInt()
			| (value       <<  0)
			| (self.branch << 16)
			| (self.rs     << 21)
		)

	def toString(self) -> str:
		target: int = self.address + self.target

		return f"{self.branch.name:7s} ${self.rs.name}, {target:#010x}".lower()

@dataclass
class BranchInstruction(Instruction):
	target: int
	rt:     Register = Register.ZERO
	rs:     Register = Register.ZERO

	def toInt(self) -> int:
		value: int = encodeSigned(self.target // 4, 16)

		return (super().toInt()
			| (value   <<  0)
			| (self.rt << 16)
			| (self.rs << 21)
		)

	def toString(self) -> str:
		target: int = self.address + self.target

		match self.opcode:
			case Opcode.BEQ | Opcode.BNE:
				args: str = f"${self.rs.name}, ${self.rt.name}, {target:#010x}"
			case _:
				args: str = f"${self.rs.name}, {target:#010x}"

		return f"{super().toString():7s} {args}".lower()

@dataclass
class JumpInstruction(Instruction):
	target: int

	def toInt(self) -> int:
		if (self.target >> 28) != (self.address >> 28):
			raise ValueError("target address out of allowed jump range")

		target: int = (self.target // 4) & 0x3ffffff

		return (super().toInt() | target)

	def toString(self) -> str:
		return f"{super().toString():7s} {self.target:#010x}"

@dataclass
class ImmInstruction(Instruction):
	value: int
	rt:    Register = Register.ZERO
	rs:    Register = Register.ZERO

	def toInt(self) -> int:
		value: int = encodeSigned(self.value, 16)

		return (super().toInt()
			| (value   <<  0)
			| (self.rt << 16)
			| (self.rs << 21)
		)

	def toString(self) -> str:
		match self.opcode:
			case Opcode.ADDI | Opcode.ADDIU | Opcode.SLTI | Opcode.SLTIU \
				| Opcode.ANDI | Opcode.ORI | Opcode.XORI:
				args: str = f"${self.rt.name}, ${self.rs.name}, {self.value:#x}"

			case Opcode.LUI:
				args: str = f"${self.rt.name}, {self.value:#x}"

			case Opcode.LWC2 | Opcode.SWC2:
				args: str = f"${self.rt}, {self.value:#x}(${self.rs.name})"

			case _:
				args: str = f"${self.rt.name}, {self.value:#x}(${self.rs.name})"

		return f"{super().toString():7s} {args}".lower()

@dataclass
class COPMoveInstruction(Instruction):
	move: COPMoveOpcode
	reg:  COP0DataRegister | GTEDataRegister | GTEControlRegister
	rt:   Register

	def toInt(self) -> int:
		return (super().toInt()
			| (self.reg  << 11)
			| (self.rt   << 16)
			| (self.move << 21)
			| (0         << 25)
		)

	def toString(self) -> str:
		inst: str = f"{self.move.name}{self.opcode.name[-1]}"

		return f"{inst:7s} ${self.rt.name}, ${self.reg.name}".lower()

@dataclass
class COP0Instruction(Instruction):
	copOpcode: COP0Opcode

	def toInt(self) -> int:
		return (super().toInt()
			| (self.copOpcode <<  0)
			| (1              << 25)
		)

	def toString(self) -> str:
		return self.copOpcode.name.lower()

@dataclass
class GTEInstruction(Instruction):
	copOpcode: GTEOpcode
	param:     int

	def toInt(self) -> int:
		return (super().toInt()
			| (self.copOpcode <<  0)
			| (self.param     <<  6)
			| (1              << 25)
		)

	def toString(self) -> str:
		return f"gte::{self.copOpcode.name:2s} {self.param:#05x}".lower()

## Instruction decoder

def parseInstruction(address: int, inst: int) -> Instruction:
	imm5:  int = (inst >>  6) & 0x1f
	imm16: int = (inst >>  0) & 0xffff
	imm19: int = (inst >>  6) & 0x7ffff
	imm20: int = (inst >>  6) & 0xfffff
	imm26: int = (inst >>  0) & 0x3ffffff

	subValue: int = (inst >>  0) & 63
	rdValue:  int = (inst >> 11) & 31
	rtValue:  int = (inst >> 16) & 31
	rsValue:  int = (inst >> 21) & 31
	opValue:  int = (inst >> 26) & 63

	copMoveValue: int = (rsValue >> 0) & 15
	copInstValue: int = (rsValue >> 4) &  1

	opcode: Opcode = Opcode(opValue)

	match opcode:
		case Opcode.SUB_INST:
			sub: SubOpcode = SubOpcode(subValue)
			rd:  Register  = Register(rdValue)
			rt:  Register  = Register(rtValue)
			rs:  Register  = Register(rsValue)

			if (sub == SubOpcode.SYSCALL) or (sub == SubOpcode.BREAK):
				return SysInstruction(address, opcode, sub, imm20)
			else:
				return RegInstruction(address, opcode, sub, imm5, rd, rt, rs)

		case Opcode.SUB_BRANCH:
			target: int             = decodeSigned(imm16, 16) * 4
			branch: SubBranchOpcode = SubBranchOpcode(rtValue)
			rs:     Register        = Register(rsValue)

			return SubBranchInstruction(address, opcode, target, branch, rs)

		case Opcode.J | Opcode.JAL:
			target: int = (address & 0xf0000000) | (imm26 * 4)

			return JumpInstruction(address, opcode, target)

		case Opcode.BEQ | Opcode.BNE | Opcode.BLEZ | Opcode.BGTZ:
			target: int       = decodeSigned(imm16, 16) * 4
			rt:     Register  = Register(rtValue)
			rs:     Register  = Register(rsValue)

			return BranchInstruction(address, opcode, target, rt, rs)

		case Opcode.ADDI | Opcode.ADDIU | Opcode.SLTI | Opcode.SLTIU:
			value: int      = decodeSigned(imm16, 16)
			rt:    Register = Register(rtValue)
			rs:    Register = Register(rsValue)

			return ImmInstruction(address, opcode, value, rt, rs)

		case Opcode.COP0:
			if copInstValue:
				cop0Opcode: COP0Opcode = COP0Opcode(subValue)

				return COP0Instruction(address, opcode, cop0Opcode)
			else:
				move:  COPMoveOpcode    = COPMoveOpcode(copMoveValue)
				cop0r: COP0DataRegister = COP0DataRegister(rdValue)
				rt:    Register         = Register(rtValue)

				return COPMoveInstruction(address, opcode, move, cop0r, rt)

		case Opcode.COP2:
			if copInstValue:
				gteOpcode: GTEOpcode = GTEOpcode(subValue)

				return GTEInstruction(address, opcode, gteOpcode, imm19)
			else:
				move: COPMoveOpcode = COPMoveOpcode(copMoveValue)
				rt:   Register      = Register(rtValue)

				if (move == COPMoveOpcode.MFC) or (opcode == COPMoveOpcode.MTC):
					gter: GTEDataRegister | GTEControlRegister = \
						GTEDataRegister(rdValue)
				else:
					gter: GTEDataRegister | GTEControlRegister = \
						GTEControlRegister(rdValue)

				return COPMoveInstruction(address, opcode, move, gter, rt)

		case _:
			rt: Register = Register(rtValue)
			rs: Register = Register(rsValue)

			return ImmInstruction(address, opcode, imm16, rt, rs)

## Executable analyzer

def parseStructFromFile(file: BinaryIO, _struct: Struct) -> tuple:
	return _struct.unpack(file.read(_struct.size))

_EXE_HEADER_STRUCT: Struct = Struct("< 8s 8x 4I 16x 2I 20x 1972s")
_EXE_HEADER_MAGIC:  bytes  = b"PS-X EXE"

_FUNCTION_RETURN: bytes = bytes.fromhex("08 00 e0 03") # jr $ra

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
		inst:   int = int.from_bytes(self.body[offset:offset + 4], "little")

		try:
			return parseInstruction(address, inst)
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
			inst:    int = int.from_bytes(self.body[offset:offset + 4], "little")

			try:
				yield parseInstruction(address, inst)
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
		for offset in self.findBytes(_FUNCTION_RETURN, start, stop, 4):
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
