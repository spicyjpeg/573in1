#!/usr/bin/env python3
# -*- coding: utf-8 -*-

__version__ = "0.3.5"
__author__  = "spicyjpeg"

import json, re
from argparse    import ArgumentParser, FileType, Namespace
from collections import defaultdict
from itertools   import chain
from pathlib     import Path
from struct      import Struct
from typing      import Any, ByteString, Generator, Mapping, Sequence
from zipfile     import ZIP_DEFLATED, ZIP_STORED, ZipFile

import lz4.block, numpy
from numpy import ndarray
from PIL   import Image

## .TIM image converter

TIM_HEADER_STRUCT:  Struct = Struct("< 2I")
TIM_SECTION_STRUCT: Struct = Struct("< I 4H")
TIM_HEADER_VERSION: int    = 0x10

LOWER_ALPHA_BOUND: int = 0x20
UPPER_ALPHA_BOUND: int = 0xe0

# Color 0x0000 is interpreted by the PS1 GPU as fully transparent, so black
# pixels must be changed to dark gray to prevent them from becoming transparent.
TRANSPARENT_COLOR: int = 0x0000
BLACK_COLOR:       int = 0x0421

def convertRGBAto16(inputData: ndarray) -> ndarray:
	source: ndarray = inputData.astype("<H")

	r:    ndarray = ((source[:, :, 0] * 249) + 1014) >> 11
	g:    ndarray = ((source[:, :, 1] * 249) + 1014) >> 11
	b:    ndarray = ((source[:, :, 2] * 249) + 1014) >> 11
	data: ndarray = r | (g << 5) | (b << 10)

	data = numpy.where(data != TRANSPARENT_COLOR, data, BLACK_COLOR)

	if source.shape[2] == 4:
		alpha: ndarray = source[:, :, 3]

		data = numpy.select(
			(
				alpha > UPPER_ALPHA_BOUND, # Leave as-is
				alpha > LOWER_ALPHA_BOUND  # Set semitransparency flag
			), (
				data,
				data | (1 << 15)
			),
			TRANSPARENT_COLOR
		)

	return data.reshape(source.shape[:-1])

def convertIndexedImage(imageObj: Image.Image) -> tuple[ndarray, ndarray]:
	# PIL/Pillow doesn't provide a proper way to get the number of colors in a
	# palette, so here's an extremely ugly hack.
	colorDepth: int   = { "RGB": 3, "RGBA": 4 }[imageObj.palette.mode]
	clutData:   bytes = imageObj.palette.tobytes()
	numColors:  int   = len(clutData) // colorDepth

	clut: ndarray = convertRGBAto16(
		numpy.frombuffer(clutData, "B").reshape(( 1, numColors, colorDepth ))
	)

	# Pad the palette to 16 or 256 colors.
	padAmount: int = (16 if (numColors <= 16) else 256) - numColors
	if padAmount:
		clut = numpy.c_[ clut, numpy.zeros(padAmount, "<H") ]

	image: ndarray = numpy.asarray(imageObj, "B")
	if image.shape[1] % 2:
		image = numpy.c_[ image, numpy.zeros(image.shape[0], "B") ]

	# Pack two pixels into each byte for 4bpp images.
	if numColors <= 16:
		image = image[:, 0::2] | (image[:, 1::2] << 4)

		if image.shape[1] % 2:
			image = numpy.c_[ image, numpy.zeros(image.shape[0], "B") ]

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
	data: bytearray = bytearray(TIM_HEADER_STRUCT.pack(TIM_HEADER_VERSION, mode))

	data.extend(TIM_SECTION_STRUCT.pack(
		TIM_SECTION_STRUCT.size + clut.size * 2,
		cx, cy, clut.shape[1], clut.shape[0]
	))
	data.extend(clut)

	data.extend(TIM_SECTION_STRUCT.pack(
		TIM_SECTION_STRUCT.size + image.size,
		ix, iy, image.shape[1] // 2, image.shape[0]
	))
	data.extend(image)

	return data

## Font metrics generator

METRICS_HEADER_STRUCT: Struct = Struct("< 3B x")
METRICS_ENTRY_STRUCT:  Struct = Struct("< 2B H")

def generateFontMetrics(
	metrics: Mapping[str, int | Mapping[str, Mapping[str, int | bool]]]
) -> bytearray:
	data: bytearray = bytearray(
		METRICS_HEADER_STRUCT.size + METRICS_ENTRY_STRUCT.size * 256
	)

	spaceWidth: int = int(metrics["spaceWidth"])
	tabWidth:   int = int(metrics["tabWidth"])
	lineHeight: int = int(metrics["lineHeight"])

	data[0:METRICS_HEADER_STRUCT.size] = \
		METRICS_HEADER_STRUCT.pack(spaceWidth, tabWidth, lineHeight)

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
			METRICS_HEADER_STRUCT.size + METRICS_ENTRY_STRUCT.size * index
		data[offset:offset + METRICS_ENTRY_STRUCT.size] = \
			METRICS_ENTRY_STRUCT.pack(x, y, w | (h << 7) | (i << 14))

	return data

## Color palette generator

PALETTE_COLOR_REGEX: re.Pattern    = re.compile(r"^#?([0-9A-Fa-f]{6})$")
PALETTE_COLORS:      Sequence[str] = (
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

PALETTE_ENTRY_STRUCT: Struct = Struct("< 3s x")

def generateColorPalette(palette: Mapping[str, str]) -> bytearray:
	data: bytearray = bytearray()

	for entry in PALETTE_COLORS:
		color: str | None = palette.get(entry, None)

		if color is None:
			raise ValueError(f"no entry found for {entry}")

		matched: re.Match | None = PALETTE_COLOR_REGEX.match(color)

		if matched is None:
			raise ValueError(f"invalid color value: {color}")

		data.extend(PALETTE_ENTRY_STRUCT.pack(bytes.fromhex(matched.group(1))))

	return data

## String table generator

TABLE_ENTRY_STRUCT: Struct = Struct("< I 2H")
TABLE_BUCKET_COUNT: int    = 256
TABLE_STRING_ALIGN: int    = 4

TABLE_ESCAPE_REGEX: re.Pattern            = re.compile(rb"\$?\{(.+?)\}")
TABLE_ESCAPE_REPL:  Mapping[bytes, bytes] = {
	b"UP_ARROW":        b"\x80",
	b"DOWN_ARROW":      b"\x81",
	b"LEFT_ARROW":      b"\x82",
	b"RIGHT_ARROW":     b"\x83",
	b"UP_ARROW_ALT":    b"\x84",
	b"DOWN_ARROW_ALT":  b"\x85",
	b"LEFT_ARROW_ALT":  b"\x86",
	b"RIGHT_ARROW_ALT": b"\x87",
	b"LEFT_BUTTON":     b"\x90",
	b"RIGHT_BUTTON":    b"\x91",
	b"START_BUTTON":    b"\x92",
	b"CLOSED_LOCK":     b"\x93",
	b"OPEN_LOCK":       b"\x94",
	b"DIR_ICON":        b"\x95",
	b"PARENT_DIR_ICON": b"\x96",
	b"FILE_ICON":       b"\x97",
	b"CHIP_ICON":       b"\x98",
	b"CART_ICON":       b"\x99"
}

def hashString(string: str) -> int:
	value: int = 0

	for byte in string.encode("ascii"):
		value = (
			byte + \
			((value <<  6) & 0xffffffff) + \
			((value << 16) & 0xffffffff) - \
			value
		) & 0xffffffff

	return value

def convertString(string: str) -> bytes:
	return TABLE_ESCAPE_REGEX.sub(
		lambda match: TABLE_ESCAPE_REPL[match.group(1).strip().upper()],
		string.encode("ascii")
	)

def prepareStrings(
		strings: Mapping[str, Any], prefix: str = ""
) -> Generator[tuple[int, bytes | None], None, None]:
	for key, value in strings.items():
		fullKey: str = prefix + key

		if value is None:
			yield hashString(fullKey), None
		elif type(value) is str:
			yield hashString(fullKey), convertString(value)
		else:
			yield from prepareStrings(value, f"{fullKey}.")

def generateStringTable(strings: Mapping[str, Any]) -> bytearray:
	offsets: dict[bytes, int]                               = {}
	chains:  defaultdict[int, list[tuple[int, int | None]]] = defaultdict(list)

	blob: bytearray = bytearray()

	for fullHash, string in prepareStrings(strings):
		if string is None:
			entry: tuple[int, int | None] = fullHash, 0
		else:
			offset: int | None = offsets.get(string, None)

			if offset is None:
				offset          = len(blob)
				offsets[string] = offset

				blob.extend(string)
				blob.append(0)

				while len(blob) % TABLE_STRING_ALIGN:
					blob.append(0)

			entry: tuple[int, int | None] = fullHash, offset

		chains[fullHash % TABLE_BUCKET_COUNT].append(entry)

	# Build the bucket array and all chains of entries.
	buckets: list[tuple[int, int | None, int]] = []
	chained: list[tuple[int, int | None, int]] = []

	for shortHash in range(TABLE_BUCKET_COUNT):
		entries: list[tuple[int, int | None]] = chains[shortHash]

		if not entries:
			buckets.append(( 0, None, 0 ))
			continue

		for index, entry in enumerate(entries):
			if index < (len(entries) - 1):
				chainIndex: int = TABLE_BUCKET_COUNT + len(chained)
			else:
				chainIndex: int = 0

			fullHash, offset = entry

			if index:
				chained.append(( fullHash, offset, chainIndex + 1 ))
			else:
				buckets.append(( fullHash, offset, chainIndex ))

	# Relocate the offsets and serialize the table.
	blobAddr: int       = TABLE_ENTRY_STRUCT.size * (len(buckets) + len(chained))
	data:     bytearray = bytearray()

	for fullHash, offset, chainIndex in chain(buckets, chained):
		absOffset: int = 0 if (offset is None) else (blobAddr + offset)

		if absOffset > 0xffff:
			raise RuntimeError("string table exceeds 64 KB size limit")

		data.extend(TABLE_ENTRY_STRUCT.pack( fullHash, absOffset, chainIndex ))

	data.extend(blob)

	return data

## Main

def createParser() -> ArgumentParser:
	parser = ArgumentParser(
		description = \
			"Parses a JSON file containing a list of resources to convert, "
			"generates the respective files and packs them into a ZIP archive.",
		add_help    = False
	)

	group = parser.add_argument_group("Tool options")
	group.add_argument(
		"-h", "--help",
		action = "help",
		help   = "Show this help message and exit"
	)

	group = parser.add_argument_group("Compression options")
	group.add_argument(
		"-c", "--compression",
		type    = str,
		choices = ( "none", "deflate", "lz4" ),
		default = "deflate",
		help    = "Set default compression algorithm (default DEFLATE)"
	)
	group.add_argument(
		"-l", "--compress-level",
		type    = int,
		default = 9,
		help    = "Set default DEFLATE and LZ4 compression level (default 9)",
		metavar = "0-9"
	)

	group = parser.add_argument_group("File paths")
	group.add_argument(
		"-s", "--source-dir",
		type = Path,
		help = \
			"Set path to directory containing source files (same directory as "
			"resource list by default)"
	)
	group.add_argument(
		"resourceList",
		type = FileType("rt"),
		help = "Path to JSON resource list",
	)
	group.add_argument(
		"output",
		type = Path,
		help = "Path to ZIP file to generate"
	)

	return parser

def main():
	parser: ArgumentParser = createParser()
	args:   Namespace      = parser.parse_args()

	with args.resourceList as _file:
		assetList: list = json.load(_file)
		sourceDir: Path = args.source_dir or Path(_file.name).parent

	with ZipFile(args.output, "w", allowZip64 = False) as _zip:
		for asset in assetList:
			match asset.get("type", "file").strip():
				case "empty":
					data: ByteString = b""

				case "text":
					with open(sourceDir / asset["source"], "rt") as _file:
						data: ByteString = _file.read().encode("ascii")

				case "binary":
					with open(sourceDir / asset["source"], "rb") as _file:
						data: ByteString = _file.read()

				case "tim":
					ix: int = int(asset["imagePos"]["x"])
					iy: int = int(asset["imagePos"]["y"])
					cx: int = int(asset["clutPos"]["x"])
					cy: int = int(asset["clutPos"]["y"])

					image: Image.Image = Image.open(sourceDir / asset["source"])
					image.load()

					if image.mode != "P":
						image = image.quantize(
							int(asset["quantize"]), dither = Image.NONE
						)

					data: ByteString = generateIndexedTIM(image, ix, iy, cx, cy)

				case "metrics":
					if "metrics" in asset:
						metrics: dict = asset["metrics"]
					else:
						with open(sourceDir / asset["source"], "rt") as _file:
							metrics: dict = json.load(_file)

					data: ByteString = generateFontMetrics(metrics)

				case "palette":
					if "palette" in asset:
						palette: dict = asset["palette"]
					else:
						with open(sourceDir / asset["source"], "rt") as _file:
							palette: dict = json.load(_file)

					data: ByteString = generateColorPalette(palette)

				case "strings":
					if "strings" in asset:
						strings: dict = asset["strings"]
					else:
						with open(sourceDir / asset["source"], "rt") as _file:
							strings: dict = json.load(_file)

					data: ByteString = generateStringTable(strings)

				case _type:
					raise KeyError(f"unsupported asset type '{_type}'")

			compressLevel: int | None = \
				asset.get("compressLevel", args.compress_level)

			match asset.get("compression", args.compression).strip():
				case "none" | None:
					_zip.writestr(asset["name"], data, ZIP_STORED)

				case "deflate":
					_zip.writestr(
						asset["name"], data, ZIP_DEFLATED, compressLevel
					)

				case "lz4":
					# ZIP archives do not "officially" support LZ4 compression,
					# so the entry is stored as an uncompressed file.
					compressed: bytes = lz4.block.compress(
						data,
						mode        = "high_compression",
						compression = compressLevel,
						store_size  = False
					)

					_zip.writestr(asset["name"], compressed, ZIP_STORED)

				case _type:
					raise KeyError(f"unsupported compression type '{_type}'")

if __name__ == "__main__":
	main()
