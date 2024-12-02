#!/usr/bin/env python3
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

__version__ = "1.0.3"
__author__  = "spicyjpeg"

import json
from argparse        import ArgumentParser, FileType, Namespace
from collections.abc import ByteString, Mapping
from pathlib         import Path
from typing          import Any

import lz4.block
from common.assets import *
from PIL           import Image

## Asset conversion

def processAsset(asset: Mapping[str, Any], sourceDir: Path) -> ByteString:
	match asset.get("type", "file").strip():
		case "empty":
			return bytes(int(asset.get("size", 0)))

		case "text" | "binary":
			with open(sourceDir / asset["source"], "rb") as file:
				data: ByteString = file.read()

			return data

		case "tim":
			ix: int = int(asset["imagePos"]["x"])
			iy: int = int(asset["imagePos"]["y"])
			cx: int = int(asset["clutPos"]["x"])
			cy: int = int(asset["clutPos"]["y"])

			image: Image.Image = Image.open(sourceDir / asset["source"])
			image.load()

			if image.mode != "P":
				image = image.quantize(
					int(asset.get("quantize", 16)), dither = Image.NONE
				)

			return generateIndexedTIM(image, ix, iy, cx, cy)

		case "metrics":
			if "metrics" in asset:
				metrics: dict = asset["metrics"]
			else:
				with open(
					sourceDir / asset["source"], "rt", encoding = "utf-8"
				) as file:
					metrics: dict = json.load(file)

			return generateFontMetrics(metrics)

		case "palette":
			if "palette" in asset:
				palette: dict = asset["palette"]
			else:
				with open(
					sourceDir / asset["source"], "rt", encoding = "utf-8"
				) as file:
					palette: dict = json.load(file)

			return generateColorPalette(palette)

		case "strings":
			if "strings" in asset:
				strings: dict = asset["strings"]
			else:
				with open(
					sourceDir / asset["source"], "rt", encoding = "utf-8"
				) as file:
					strings: dict = json.load(file)

			return generateStringTable(strings)

		case "db":
			if "db" in asset:
				db: dict = asset["db"]
			else:
				with open(
					sourceDir / asset["source"], "rt", encoding = "utf-8"
				) as file:
					db: dict = json.load(file)

			# TODO: implement
			return b""

		case _type:
			raise KeyError(f"unsupported asset type '{_type}'")

## Main

def createParser() -> ArgumentParser:
	parser = ArgumentParser(
		description = \
			"Parses a JSON file containing a list of resources to convert, "
			"generates the respective files and packs them into a 573in1 "
			"resource package (.pkg file).",
		add_help    = False
	)

	group = parser.add_argument_group("Tool options")
	group.add_argument(
		"-h", "--help",
		action = "help",
		help   = "Show this help message and exit"
	)

	group = parser.add_argument_group("Package options")
	group.add_argument(
		"-a", "--align",
		type    = int,
		default = 2048,
		help    = \
			"Ensure all files in the package are aligned to specified sector "
			"size (default 2048)",
		metavar = "length"

	)
	group.add_argument(
		"-c", "--compress-level",
		type    = int,
		default = 9,
		help    = \
			"Set default LZ4 compression level (0 to disable compression, "
			"default 9)",
		metavar = "0-9"
	)

	group = parser.add_argument_group("File paths")
	group.add_argument(
		"-s", "--source-dir",
		type    = Path,
		help    = \
			"Set path to directory containing source files (same directory as "
			"resource list by default)",
		metavar = "dir"
	)
	group.add_argument(
		"configFile",
		type = FileType("rt", encoding = "utf-8"),
		help = "Path to JSON configuration file",
	)
	group.add_argument(
		"output",
		type = FileType("wb"),
		help = "Path to package file to generate"
	)

	return parser

def main():
	parser: ArgumentParser = createParser()
	args:   Namespace      = parser.parse_args()

	with args.configFile as file:
		configFile: dict[str, Any] = json.load(file)
		sourceDir:  Path           = \
			args.source_dir or Path(file.name).parent

	entries:  dict[str, PackageIndexEntry] = {}
	fileData: bytearray                    = bytearray()

	for asset in configFile["resources"]:
		data:  ByteString        = processAsset(asset, sourceDir)
		entry: PackageIndexEntry = \
			PackageIndexEntry(len(fileData), 0, len(data))

		compLevel: int | None = asset.get("compLevel", args.compress_level)

		if data and compLevel:
			data = lz4.block.compress(
				data,
				mode        = "high_compression",
				compression = compLevel,
				store_size  = False
			)
			entry.compLength = len(data)

		entries[asset["name"]] = entry
		fileData           += data

		while len(fileData) % args.align:
			fileData.append(0)

	indexData: bytearray = generatePackageIndex(entries, args.align)

	while len(indexData) % args.align:
		indexData.append(0)

	with args.output as file:
		file.write(indexData)
		file.write(fileData)

if __name__ == "__main__":
	main()
