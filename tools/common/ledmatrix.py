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

from collections.abc import Mapping
from struct          import Struct
from typing          import Any

import numpy as np
from numpy.typing import NDArray
from PIL          import Image
from .util        import HashTableBuilder, StringBlobBuilder

## LED matrix image generator

_LED_IMAGE_HEADER_STRUCT: Struct = Struct("< 8s 3H 2x")
_LED_IMAGE_HEADER_MAGIC:  bytes  = b"573ldimg"

_LED_MATRIX_COLORS: NDArray[np.uint32] = np.array((
	  0,   0, 0, 255, # LED_BLACK
	255,   0, 0, 255, # LED_RED
	  0, 255, 0, 255, # LED_GREEN
	255, 255, 0, 255  # LED_YELLOW
), "B").view("I")

def _convertLEDImage(imageObj: Image.Image) -> NDArray[np.uint32]:
	source: NDArray[np.uint8] = np.asarray(imageObj.convert("RGBA"), "B")
	source                    = \
		source.view("I").reshape(( -1, source.shape[1] ))

	order: NDArray = _LED_MATRIX_COLORS.argsort()
	image: NDArray = _LED_MATRIX_COLORS.searchsorted(source, "left", order)
	image          = order[image].astype("B")

	if (_LED_MATRIX_COLORS[image] != source).any():
		raise RuntimeError(
			"source image contains colors that cannot be displayed on LED dot "
			"matrix"
		)

	# Pack 16 pixels into each word (4 pixels into each byte).
	unaligned: int = image.shape[1] % 16

	if unaligned:
		image = np.c_[
			image,
			np.zeros(( 16 - unaligned, 1 ), "B")
		]

	p0: NDArray[np.uint8] = image[:, 0::4]
	p1: NDArray[np.uint8] = image[:, 1::4]
	p2: NDArray[np.uint8] = image[:, 2::4]
	p3: NDArray[np.uint8] = image[:, 3::4]

	return (p0 | (p1 << 2) | (p2 << 4) | (p3 << 6)).view("<I")

def generateLEDImage(imageObj: Image.Image) -> bytes:
	image: NDArray[np.uint32] = _convertLEDImage(imageObj)

	return _LED_IMAGE_HEADER_STRUCT.pack(
		_LED_IMAGE_HEADER_MAGIC,
		imageObj.width,
		imageObj.height,
		image.shape[1] * image.itemsize
	) + image.tobytes()

## LED matrix font generator

_LED_FONT_HEADER_STRUCT: Struct = Struct("< 8s 3B b 2H")
_LED_FONT_HEADER_MAGIC:  bytes  = b"573ldfnt"
_LED_FONT_ENTRY_STRUCT:  Struct = Struct("< I 2B H")

_LUMA_THRESHOLD:  int = 128
_ALPHA_THRESHOLD: int = 128

def _convertGlyph(
	imageObj:    Image.Image,
	ignoreAlpha: bool = False
) -> NDArray[np.uint16]:
	image: NDArray[np.uint8] = np.asarray(imageObj, "B")
	bits:  NDArray[np.bool_] = (image[:, :, 0] >= _LUMA_THRESHOLD)

	if not ignoreAlpha:
		bits &= (image[:, :, 1] >= _ALPHA_THRESHOLD)

	# Glyph bitmaps are stored as a series of 16-bit bitfields, each
	# representing one column of pixels (with the LSB being the top pixel).
	bits = np.packbits(bits, 0, "little").T.astype("<H")

	if bits.shape[1] == 2:
		bits = bits[:, 0] | (bits[:, 1] << 8)
	elif bits.shape[1] != 1:
		raise RuntimeError("cannot pack >16 pixel tall glyphs")

	return bits

def generateLEDFont(
	imageObj:   Image.Image,
	metrics:    Mapping[str, Any],
	numBuckets: int = 128
) -> bytearray:
	imageObj = imageObj.convert("LA")

	spaceWidth:     int = int(metrics["spaceWidth"])
	tabWidth:       int = int(metrics["tabWidth"])
	lineHeight:     int = int(metrics["lineHeight"])
	baselineOffset: int = int(metrics["baselineOffset"])

	hashTable: HashTableBuilder  = HashTableBuilder(numBuckets)
	blob:      StringBlobBuilder = StringBlobBuilder(2)

	for ch, entry in metrics["characterSizes"].items():
		x: int  = int(entry["x"])
		y: int  = int(entry["y"])
		w: int  = int(entry["width"])
		h: int  = int(entry["height"])

		if (w < 0) or (w > 255) or (h < 1) or (h > 16):
			raise ValueError("all characters must be <=255x16 pixels")
		if h > lineHeight:
			raise ValueError("character height exceeds line height")

		cropped: Image.Image = imageObj.crop(( x, y, x + w, y + h ))
		glyph:   bytes       = _convertGlyph(cropped).tobytes()

		hashTable.addEntry(ord(ch), ( w, h, blob.addString(glyph) ))

	blobOffset: int = 0 \
		+ _LED_FONT_HEADER_STRUCT.size \
		+ _LED_FONT_ENTRY_STRUCT.size * len(hashTable.entries)

	tableData: bytearray = bytearray()
	tableData           += _LED_FONT_HEADER_STRUCT.pack(
		_LED_FONT_HEADER_MAGIC,
		spaceWidth,
		tabWidth,
		lineHeight,
		baselineOffset,
		numBuckets,
		len(hashTable.entries)
	)

	for entry in hashTable.entries:
		if entry is None:
			tableData += bytes(_LED_FONT_ENTRY_STRUCT.size)
		else:
			w, h, offset = entry.data
			tableData   += _LED_FONT_ENTRY_STRUCT.pack(
				entry.fullHash | (entry.chainIndex << 21),
				w,
				h,
				blobOffset + offset
			)

	return tableData + blob.data
