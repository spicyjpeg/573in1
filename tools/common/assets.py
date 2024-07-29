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

from itertools import chain
from struct    import Struct
from typing    import Any, Generator, Mapping, Sequence

import numpy
from numpy import ndarray
from PIL   import Image
from .util import colorFromString, generateHashTable, hashData

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

	image, clut     = convertIndexedImage(imageObj)
	data: bytearray = bytearray()

	data += _TIM_HEADER_STRUCT.pack(
		_TIM_HEADER_VERSION,
		0x8 if (clut.size <= 16) else 0x9
	)

	data += _TIM_SECTION_STRUCT.pack(
		_TIM_SECTION_STRUCT.size + clut.size * 2,
		cx,
		cy,
		clut.shape[1],
		clut.shape[0]
	)
	data.extend(clut)

	data += _TIM_SECTION_STRUCT.pack(
		_TIM_SECTION_STRUCT.size + image.size,
		ix,
		iy,
		image.shape[1] // 2,
		image.shape[0]
	)
	data.extend(image)

	return data

## Font metrics generator

_METRICS_HEADER_STRUCT: Struct = Struct("< 3B b")
_METRICS_ENTRY_STRUCT:  Struct = Struct("< 2I")
_METRICS_BUCKET_COUNT:  int    = 256

def generateFontMetrics(metrics: Mapping[str, Any]) -> bytearray:
	spaceWidth:     int = int(metrics["spaceWidth"])
	tabWidth:       int = int(metrics["tabWidth"])
	lineHeight:     int = int(metrics["lineHeight"])
	baselineOffset: int = int(metrics["baselineOffset"])

	entries: dict[int, int] = {}

	for ch, entry in metrics["characterSizes"].items():
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

		entries[ord(ch)] = (0
			| (x <<  0)
			| (y <<  8)
			| (w << 16)
			| (h << 23)
			| (i << 30)
		)

	buckets, chained = generateHashTable(entries, _METRICS_BUCKET_COUNT)
	table: bytearray = bytearray()

	if (len(buckets) + len(chained)) > 2048:
		raise RuntimeError("font hash table must have <=2048 entries")

	table += _METRICS_HEADER_STRUCT.pack(
		spaceWidth,
		tabWidth,
		lineHeight,
		baselineOffset
	)

	for entry in chain(buckets, chained):
		if entry is None:
			table += _METRICS_ENTRY_STRUCT.pack(0, 0)
			continue

		table += _METRICS_ENTRY_STRUCT.pack(
			entry.fullHash | (entry.chainIndex << 21),
			entry.data
		)

	return table

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

		data += _PALETTE_ENTRY_STRUCT.pack(r, g, b)

	return data

## String table generator

_STRING_TABLE_ENTRY_STRUCT: Struct = Struct("< I 2H")
_STRING_TABLE_BUCKET_COUNT: int    = 256
_STRING_TABLE_ALIGNMENT:    int    = 4

def _walkStringTree(
		strings: Mapping[str, Any], prefix: str = ""
) -> Generator[tuple[int, bytes | None], None, None]:
	for key, value in strings.items():
		fullKey: str = prefix + key
		keyHash: int = hashData(fullKey.encode("ascii"))

		if value is None:
			yield keyHash, None
		elif isinstance(value, str):
			yield keyHash, value.encode("utf-8")
		else:
			yield from _walkStringTree(value, f"{fullKey}.")

def generateStringTable(strings: Mapping[str, Any]) -> bytearray:
	offsets: dict[bytes, int] = {}
	entries: dict[int, int]   = {}
	blob:    bytearray        = bytearray()

	for keyHash, string in _walkStringTree(strings):
		if string is None:
			entries[keyHash] = 0
			continue

		# Identical strings associated to multiple keys are deduplicated.
		offset: int | None = offsets.get(string, None)

		if offset is None:
			offset          = len(blob)
			offsets[string] = offset

			blob += string
			blob.append(0)

			while len(blob) % _STRING_TABLE_ALIGNMENT:
				blob.append(0)

		entries[keyHash] = offset

	buckets, chained = generateHashTable(entries, _STRING_TABLE_BUCKET_COUNT)
	table: bytearray = bytearray()

	# Relocate the offsets and serialize the table.
	blobOffset: int = \
		(len(buckets) + len(chained)) * _STRING_TABLE_ENTRY_STRUCT.size

	for entry in chain(buckets, chained):
		if entry is None:
			table += _STRING_TABLE_ENTRY_STRUCT.pack(0, 0, 0)
			continue

		table += _STRING_TABLE_ENTRY_STRUCT.pack(
			entry.fullHash,
			0 if (entry.data is None) else (blobOffset + entry.data),
			entry.chainIndex
		)

	return table + blob
