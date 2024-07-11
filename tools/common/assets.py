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

import re
from collections import defaultdict
from itertools   import chain
from struct      import Struct
from typing      import Any, Generator, Mapping, Sequence

import numpy
from numpy import ndarray
from PIL   import Image
from .util import colorFromString, hashData

## .TIM image converter

_TIM_HEADER_STRUCT:  Struct = Struct("< 2I")
_TIM_SECTION_STRUCT: Struct = Struct("< I 4H")
_TIM_HEADER_VERSION: int    = 0x10

_LOWER_ALPHA_BOUND: int = 0x20
_UPPER_ALPHA_BOUND: int = 0xe0

# Color 0x0000 is interpreted by the PS1 GPU as fully transparent, so black
# pixels must be changed to dark gray to prevent them from becoming transparent.
_TRANSPARENT_COLOR: int = 0x0000
_BLACK_COLOR:       int = 0x0421

def _convertRGBAto16(inputData: ndarray) -> ndarray:
	source: ndarray = inputData.astype("<H")

	r:    ndarray = ((source[:, :, 0] * 249) + 1014) >> 11
	g:    ndarray = ((source[:, :, 1] * 249) + 1014) >> 11
	b:    ndarray = ((source[:, :, 2] * 249) + 1014) >> 11
	data: ndarray = r | (g << 5) | (b << 10)

	data = numpy.where(data != _TRANSPARENT_COLOR, data, _BLACK_COLOR)

	if source.shape[2] == 4:
		alpha: ndarray = source[:, :, 3]

		data = numpy.select(
			(
				alpha > _UPPER_ALPHA_BOUND, # Leave as-is
				alpha > _LOWER_ALPHA_BOUND  # Set semitransparency flag
			), (
				data,
				data | (1 << 15)
			),
			_TRANSPARENT_COLOR
		)

	return data.reshape(source.shape[:-1])

def convertIndexedImage(imageObj: Image.Image) -> tuple[ndarray, ndarray]:
	# PIL/Pillow doesn't provide a proper way to get the number of colors in a
	# palette, so here's an extremely ugly hack.
	colorDepth: int   = { "RGB": 3, "RGBA": 4 }[imageObj.palette.mode]
	clutData:   bytes = imageObj.palette.tobytes()
	numColors:  int   = len(clutData) // colorDepth

	clut: ndarray = _convertRGBAto16(
		numpy.frombuffer(clutData, "B").reshape(( 1, numColors, colorDepth ))
	)

	# Pad the palette to 16 or 256 colors.
	padAmount: int = (16 if (numColors <= 16) else 256) - numColors
	if padAmount:
		clut = numpy.c_[ clut, numpy.zeros(( 1, padAmount ), "<H") ]

	image: ndarray = numpy.asarray(imageObj, "B")
	if image.shape[1] % 2:
		image = numpy.c_[ image, numpy.zeros((image.shape[0], 1 ), "B") ]

	# Pack two pixels into each byte for 4bpp images.
	if numColors <= 16:
		image = image[:, 0::2] | (image[:, 1::2] << 4)

		if image.shape[1] % 2:
			image = numpy.c_[ image, numpy.zeros(( image.shape[0], 1 ), "B") ]

	return image, clut

def generateIndexedTIM(
	imageObj: Image.Image, ix: int, iy: int, cx: int, cy: int
) -> bytearray:
	if (ix < 0) or (ix > 1023) or (iy < 0) or (iy > 1023):
		raise ValueError("image X/Y coordinates must be in 0-1023 range")
	if (cx < 0) or (cx > 1023) or (cy < 0) or (cy > 1023):
		raise ValueError("palette X/Y coordinates must be in 0-1023 range")

	image, clut = convertIndexedImage(imageObj)

	mode: int       = 0x8 if (clut.size <= 16) else 0x9
	data: bytearray = bytearray(
		_TIM_HEADER_STRUCT.pack(_TIM_HEADER_VERSION, mode)
	)

	data.extend(_TIM_SECTION_STRUCT.pack(
		_TIM_SECTION_STRUCT.size + clut.size * 2,
		cx, cy, clut.shape[1], clut.shape[0]
	))
	data.extend(clut)

	data.extend(_TIM_SECTION_STRUCT.pack(
		_TIM_SECTION_STRUCT.size + image.size,
		ix, iy, image.shape[1] // 2, image.shape[0]
	))
	data.extend(image)

	return data

## Font metrics generator

_METRICS_HEADER_STRUCT: Struct = Struct("< 3B x")
_METRICS_ENTRY_STRUCT:  Struct = Struct("< 2B H")

def generateFontMetrics(metrics: Mapping[str, Any]) -> bytearray:
	data: bytearray = bytearray(
		_METRICS_HEADER_STRUCT.size + _METRICS_ENTRY_STRUCT.size * 256
	)

	spaceWidth: int = int(metrics["spaceWidth"])
	tabWidth:   int = int(metrics["tabWidth"])
	lineHeight: int = int(metrics["lineHeight"])

	data[0:_METRICS_HEADER_STRUCT.size] = \
		_METRICS_HEADER_STRUCT.pack(spaceWidth, tabWidth, lineHeight)

	for ch, entry in metrics["characterSizes"].items():
		index: int = ord(ch)
		#index: int = ch.encode("ascii")[0]

		if (index < 0) or (index > 255):
			raise ValueError(f"extended character {index} is not supported")

		x: int  = int(entry["x"])
		y: int  = int(entry["y"])
		w: int  = int(entry["width"])
		h: int  = int(entry["height"])
		i: bool = bool(entry.get("icon", False))

		if (x < 0) or (x > 255) or (y < 0) or (y > 255):
			raise ValueError("all X/Y coordinates must be in 0-255 range")
		if (w < 0) or (w > 127) or (h < 0) or (h > 127):
			raise ValueError("all characters must be <=127x127 pixels")
		if h > lineHeight:
			raise ValueError("character height exceeds line height")

		offset: int = \
			_METRICS_HEADER_STRUCT.size + _METRICS_ENTRY_STRUCT.size * index
		data[offset:offset + _METRICS_ENTRY_STRUCT.size] = \
			_METRICS_ENTRY_STRUCT.pack(x, y, w | (h << 7) | (i << 14))

	return data

## Color palette generator

_PALETTE_ENTRY_STRUCT: Struct = Struct("< 3B x")

_PALETTE_ENTRIES: Sequence[str] = (
	"default",
	"shadow",
	"backdrop",
	"accent1",
	"accent2",
	"window1",
	"window2",
	"window3",
	"highlight1",
	"highlight2",
	"progress1",
	"progress2",
	"box1",
	"box2",
	"text1",
	"text2",
	"title",
	"subtitle"
)

def generateColorPalette(
	palette: Mapping[str, str | Sequence[int]]
) -> bytearray:
	data: bytearray = bytearray()

	for entry in _PALETTE_ENTRIES:
		color: str | Sequence[int] | None = palette.get(entry, None)

		if color is None:
			raise ValueError(f"no entry found for {entry}")
		if isinstance(color, str):
			r, g, b = colorFromString(color)
		else:
			r, g, b = color

		data.extend(_PALETTE_ENTRY_STRUCT.pack(r, g, b))

	return data

## String table generator

_TABLE_ENTRY_STRUCT: Struct = Struct("< I 2H")
_TABLE_BUCKET_COUNT: int    = 256
_TABLE_STRING_ALIGN: int    = 4

_TABLE_ESCAPE_REGEX: re.Pattern            = re.compile(rb"\$?\{(.+?)\}")
_TABLE_ESCAPE_REPL:  Mapping[bytes, bytes] = {
	b"UP_ARROW":        b"\x80",
	b"DOWN_ARROW":      b"\x81",
	b"LEFT_ARROW":      b"\x82",
	b"RIGHT_ARROW":     b"\x83",
	b"UP_ARROW_ALT":    b"\x84",
	b"DOWN_ARROW_ALT":  b"\x85",
	b"LEFT_ARROW_ALT":  b"\x86",
	b"RIGHT_ARROW_ALT": b"\x87",

	b"LEFT_BUTTON":  b"\x90",
	b"RIGHT_BUTTON": b"\x91",
	b"START_BUTTON": b"\x92",
	b"CLOSED_LOCK":  b"\x93",
	b"OPEN_LOCK":    b"\x94",
	b"CHIP_ICON":    b"\x95",
	b"CART_ICON":    b"\x96",

	b"CDROM_ICON":      b"\xa0",
	b"HDD_ICON":        b"\xa1",
	b"HOST_ICON":       b"\xa2",
	b"DIR_ICON":        b"\xa3",
	b"PARENT_DIR_ICON": b"\xa4",
	b"FILE_ICON":       b"\xa5"
}

def _convertString(string: str) -> bytes:
	return _TABLE_ESCAPE_REGEX.sub(
		lambda match: _TABLE_ESCAPE_REPL[match.group(1).strip().upper()],
		string.encode("ascii")
	)

def _walkStringTree(
		strings: Mapping[str, Any], prefix: str = ""
) -> Generator[tuple[int, bytes | None], None, None]:
	for key, value in strings.items():
		fullKey: str = prefix + key

		if value is None:
			yield hashData(fullKey.encode("ascii")), None
		elif isinstance(value, str):
			yield hashData(fullKey.encode("ascii")), _convertString(value)
		else:
			yield from _walkStringTree(value, f"{fullKey}.")

def generateStringTable(strings: Mapping[str, Any]) -> bytearray:
	offsets: dict[bytes, int]                               = {}
	chains:  defaultdict[int, list[tuple[int, int | None]]] = defaultdict(list)

	blob: bytearray = bytearray()

	for fullHash, string in _walkStringTree(strings):
		if string is None:
			entry: tuple[int, int | None] = fullHash, 0
		else:
			offset: int | None = offsets.get(string, None)

			if offset is None:
				offset          = len(blob)
				offsets[string] = offset

				blob.extend(string)
				blob.append(0)

				while len(blob) % _TABLE_STRING_ALIGN:
					blob.append(0)

			entry: tuple[int, int | None] = fullHash, offset

		chains[fullHash % _TABLE_BUCKET_COUNT].append(entry)

	# Build the bucket array and all chains of entries.
	buckets: list[tuple[int, int | None, int]] = []
	chained: list[tuple[int, int | None, int]] = []

	for shortHash in range(_TABLE_BUCKET_COUNT):
		entries: list[tuple[int, int | None]] = chains[shortHash]

		if not entries:
			buckets.append(( 0, None, 0 ))
			continue

		for index, entry in enumerate(entries):
			if index < (len(entries) - 1):
				chainIndex: int = _TABLE_BUCKET_COUNT + len(chained)
			else:
				chainIndex: int = 0

			fullHash, offset = entry

			if index:
				chained.append(( fullHash, offset, chainIndex + 1 ))
			else:
				buckets.append(( fullHash, offset, chainIndex ))

	# Relocate the offsets and serialize the table.
	totalLength: int       = len(buckets) + len(chained)
	blobOffset:  int       = _TABLE_ENTRY_STRUCT.size * totalLength
	data:        bytearray = bytearray()

	for fullHash, offset, chainIndex in chain(buckets, chained):
		absOffset: int = 0 if (offset is None) else (blobOffset + offset)

		if absOffset > 0xffff:
			raise RuntimeError("string table exceeds 64 KB size limit")

		data.extend(_TABLE_ENTRY_STRUCT.pack(fullHash, absOffset, chainIndex))

	data.extend(blob)

	return data
