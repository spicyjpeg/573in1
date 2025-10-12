# -*- coding: utf-8 -*-

# 573in1 - Copyright (C) 2022-2025 spicyjpeg
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

from dataclasses import dataclass, field
from enum        import IntEnum, IntFlag
from struct      import Struct
from typing      import Self

import numpy
from numpy   import ndarray
from PIL     import Image

## Input image handling

# Pillow's built-in quantize() method will use different algorithms, some of
# which are broken, depending on whether the input image has an alpha channel.
# As a workaround, all images are "quantized" (with no dithering - images with
# more colors than allowed are rejected) manually instead. This conversion is
# performed on indexed color images as well in order to normalize their
# palettes.
def quantizeImage(imageObj: Image.Image, numColors: int) -> Image.Image:
	#if imageObj.mode == "P":
		#return imageObj
	if imageObj.mode not in ( "RGB", "RGBA" ):
		imageObj = imageObj.convert("RGBA")

	image: ndarray = numpy.asarray(imageObj, "B")
	clut, image    = numpy.unique(
		image.reshape(( -1, image.shape[2] )),
		return_inverse = True,
		axis           = 0
	)

	if clut.shape[0] > numColors:
		raise RuntimeError(
			f"source image contains {clut.shape[0]} unique colors (must be "
			f"{numColors} or less)"
		)

	image = image.astype("B").reshape((
		imageObj.height,
		imageObj.width
	))
	newObj: Image.Image = Image.fromarray(image, "P")

	newObj.putpalette(clut.tobytes(), imageObj.mode)
	return newObj

def _getImagePalette(imageObj: Image.Image) -> ndarray:
	clut: ndarray = numpy.array(imageObj.getpalette("RGBA"), "B")
	clut          = clut.reshape(( -1, 4 ))

	# Pillow's PNG decoder does not handle indexed color images with alpha
	# correctly, so a workaround is needed here to manually integrate the
	# contents of the image's "tRNs" chunk into the palette.
	if "transparency" in imageObj.info:
		alpha: bytes = imageObj.info["transparency"]
		clut[:, 3]   = numpy.frombuffer(alpha.ljust(clut.shape[0], b"\xff"))

	return clut

## RGBA to 16bpp colorspace conversion

_LOWER_ALPHA_BOUND: int = 32
_UPPER_ALPHA_BOUND: int = 224

# Color 0x0000 is interpreted by the PS1 GPU as fully transparent, so black
# pixels must be changed to dark gray to prevent them from becoming transparent.
_TRANSPARENT_COLOR: int = 0x0000
_BLACK_COLOR:       int = 0x0421

def _to16bpp(inputData: ndarray, forceSTP: bool = False) -> ndarray:
	source: ndarray = inputData.astype("<H")
	r:      ndarray = ((source[:, :, 0] * 31) + 127) // 255
	g:      ndarray = ((source[:, :, 1] * 31) + 127) // 255
	b:      ndarray = ((source[:, :, 2] * 31) + 127) // 255

	solid:           ndarray = r | (g << 5) | (b << 10)
	semitransparent: ndarray = solid | (1 << 15)

	data: ndarray = numpy.full_like(solid, _TRANSPARENT_COLOR)

	if source.shape[2] == 4:
		alpha: ndarray = source[:, :, 3]
	else:
		alpha: ndarray = numpy.full(source.shape[:-1], 0xff, "B")

	numpy.copyto(data, semitransparent, where = (alpha >= _LOWER_ALPHA_BOUND))

	if not forceSTP:
		numpy.copyto(data, solid, where = (alpha >= _UPPER_ALPHA_BOUND))
		numpy.copyto(
			data,
			_BLACK_COLOR,
			where = (
				(alpha >= _UPPER_ALPHA_BOUND) &
				(solid == _TRANSPARENT_COLOR)
			)
		)

	return data

def _convertIndexedImage(
	imageObj: Image.Image,
	forceSTP: bool = False
) -> tuple[ndarray, ndarray]:
	clut:      ndarray = _getImagePalette(imageObj)
	numColors: int     = clut.shape[0]
	padAmount: int     = (16 if (numColors <= 16) else 256) - numColors

	# Pad the palette to 16 or 256 colors after converting it to 16bpp.
	clut = _to16bpp(clut.reshape(( 1, -1, 4 )), forceSTP)

	if padAmount:
		clut = numpy.c_[
			clut,
			numpy.zeros(( 1, padAmount ), "<H")
		]

	image: ndarray = numpy.asarray(imageObj, "B")

	if image.shape[1] % 2:
		image = numpy.c_[
			image,
			numpy.zeros(( imageObj.height, 1 ), "B")
		]

	# Pack two pixels into each byte for 4bpp images.
	if numColors <= 16:
		image = image[:, 0::2] | (image[:, 1::2] << 4)

		if image.shape[1] % 2:
			image = numpy.c_[
				image,
				numpy.zeros(( imageObj.height, 1 ), "B")
			]

	return image, clut

## .TIM file generator

class TIMColorDepth(IntEnum):
	COLOR_4BPP  = 0
	COLOR_8BPP  = 1
	COLOR_16BPP = 2

class TIMHeaderFlag(IntFlag):
	COLOR_BITMASK = 3 << 0
	HAS_PALETTE   = 1 << 3

_TIM_HEADER_STRUCT:  Struct = Struct("< 2I")
_TIM_SECTION_STRUCT: Struct = Struct("< I 4H")
_TIM_HEADER_VERSION: int    = 0x10
_CLT_HEADER_VERSION: int    = 0x11
_PXL_HEADER_VERSION: int    = 0x12

@dataclass
class TIMSection:
	x:    int
	y:    int
	data: ndarray = field(repr = False)

	@staticmethod
	def parse(
		data:      bytes | bytearray,
		offset:    int = 0,
		pixelSize: int = 2
	) -> tuple[int, Self]:
		(
			endOffset,
			x,
			y,
			width,
			height
		) = _TIM_SECTION_STRUCT.unpack_from(data, offset)

		data    = data[offset + _TIM_SECTION_STRUCT.size:offset + endOffset]
		offset += endOffset

		if len(data) != (width * height * 2):
			raise RuntimeError("section length does not match image size")

		image: ndarray = numpy.frombuffer(
			data,
			"<H" if (pixelSize == 2) else "B"
		).reshape((
			height,
			(width * pixelSize) // 2
		))

		return offset, TIMSection(x, y, image)

	def serialize(self) -> bytes:
		if (self.x < 0) or (self.x > 1023) or (self.y < 0) or (self.y > 1023):
			raise ValueError("section X/Y coordinates must be in 0-1023 range")

		data: bytes = self.data.tobytes()

		if len(data) % 4:
			raise RuntimeError("section data should be aligned to 4 bytes")

		return _TIM_SECTION_STRUCT.pack(
			_TIM_SECTION_STRUCT.size + len(data),
			self.x,
			self.y,
			(self.data.shape[1] * self.data.itemsize) // 2,
			self.data.shape[0]
		) + data

@dataclass
class TIMImage:
	colorDepth: TIMColorDepth
	image:      TIMSection | None = None
	clut:       TIMSection | None = None

	@staticmethod
	def fromRawImage(
		imageObj: Image.Image,
		imageX:   int,
		imageY:   int,
		forceSTP: bool = False
	) -> Self:
		image: ndarray = numpy.asarray(imageObj, "B")
		image          = _to16bpp(image, forceSTP)

		return TIMImage(
			TIMColorDepth.COLOR_16BPP,
			TIMSection(imageX, imageY, image)
		)

	@staticmethod
	def fromIndexedImage(
		imageObj: Image.Image,
		imageX:   int,
		imageY:   int,
		clutX:    int,
		clutY:    int,
		forceSTP: bool = False
	) -> Self:
		image, clut = _convertIndexedImage(imageObj, forceSTP)

		if clut.size <= 16:
			colorDepth: TIMColorDepth = TIMColorDepth.COLOR_4BPP
		else:
			colorDepth: TIMColorDepth = TIMColorDepth.COLOR_8BPP

		return TIMImage(
			colorDepth,
			TIMSection(imageX, imageY, image),
			TIMSection(clutX,  clutY,  clut)
		)

	@staticmethod
	def parse(data: bytes | bytearray, offset: int = 0) -> Self:
		version, flags = _TIM_HEADER_STRUCT.unpack_from(data, offset)
		offset        += _TIM_HEADER_STRUCT.size

		if version == _TIM_HEADER_VERSION:
			hasImage: bool = True
			hasCLUT:  bool = bool(flags & TIMHeaderFlag.HAS_PALETTE)
		else:
			hasImage: bool = (version == _PXL_HEADER_VERSION)
			hasCLUT:  bool = (version == _CLT_HEADER_VERSION)

			if not (hasImage or hasCLUT):
				raise ValueError(f"invalid .TIM file version: {version:#x}")

		colorDepth: TIMColorDepth = \
			TIMColorDepth(flags & TIMHeaderFlag.COLOR_BITMASK)
		pixelSize:  int           = \
			2 if (colorDepth == TIMColorDepth.COLOR_16BPP) else 1

		tim: TIMImage = TIMImage(colorDepth)

		if hasCLUT:
			offset, tim.clut  = TIMSection.parse(data, offset, 2)
		if hasImage:
			offset, tim.image = TIMSection.parse(data, offset, pixelSize)

		return tim

	def serialize(self) -> bytearray:
		match self.colorDepth, self.image, self.clut:
			case _, None, None:
				raise ValueError("at least one section is required")

			case TIMColorDepth.COLOR_4BPP | TIMColorDepth.COLOR_8BPP, _, None:
				version: int = _PXL_HEADER_VERSION
				flags:   int = int(self.colorDepth)

			case TIMColorDepth.COLOR_4BPP | TIMColorDepth.COLOR_8BPP, None, _:
				version: int = _CLT_HEADER_VERSION
				flags:   int = int(TIMColorDepth.COLOR_16BPP)

			case TIMColorDepth.COLOR_4BPP | TIMColorDepth.COLOR_8BPP, _, _:
				version: int = _TIM_HEADER_VERSION
				flags:   int = \
					int(self.colorDepth) | int(TIMHeaderFlag.HAS_PALETTE)

			case TIMColorDepth.COLOR_16BPP, _, None:
				version: int = _TIM_HEADER_VERSION
				flags:   int = int(TIMColorDepth.COLOR_16BPP)

			case TIMColorDepth.COLOR_16BPP, _, _:
				raise ValueError("16bpp images cannot have a palette section")

		data: bytearray = bytearray()
		data           += _TIM_HEADER_STRUCT.pack(version, flags)

		if self.clut  is not None:
			data += self.clut.serialize()
		if self.image is not None:
			data += self.image.serialize()

		return data

## LED matrix image generator

_LED_IMAGE_HEADER_STRUCT: Struct = Struct("< 8s 2H")
_LED_IMAGE_HEADER_MAGIC:  bytes  = b"573ledim"

_LED_MATRIX_COLORS: ndarray = numpy.array((
	(   0,   0, 0 ), # LED_BLACK
	( 255,   0, 0 ), # LED_RED
	(   0, 255, 0 ), # LED_GREEN
	( 255, 255, 0 )  # LED_YELLOW
), numpy.uint8)

def generateLEDImage(imageObj: Image.Image) -> bytearray:
	# TODO: implement

	data: bytearray = bytearray()

	return data
