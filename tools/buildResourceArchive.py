#!/usr/bin/env python3
# -*- coding: utf-8 -*-

__version__ = "0.4.1"
__author__  = "spicyjpeg"

import json
from argparse    import ArgumentParser, FileType, Namespace
from pathlib     import Path
from typing      import ByteString
from zipfile     import ZIP_DEFLATED, ZIP_STORED, ZipFile

import lz4.block
from common.assets import *
from PIL           import Image

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
