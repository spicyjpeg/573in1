#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""ISO9660 CD-ROM image generator

A simple tool for building ISO9660 CD-ROM images with 2048-byte sectors, as used
by the PlayStation 1 and other machines, from a JSON configuration file
describing their contents. Unlike most other ISO9660 authoring tools, files and
directories are placed in the image in the same order they are specified in the
JSON file, allowing for customization of the disc's layout. Requires pycdlib to
be installed.
"""

__version__ = "0.1.0"
__author__  = "spicyjpeg"

import json, re, sys
from argparse import ArgumentParser, FileType, Namespace
from pathlib  import Path
from typing   import Any, BinaryIO, Iterable

from pycdlib.dr      import DirectoryRecord
from pycdlib.inode   import Inode
from pycdlib.pycdlib import PyCdlib

## Utilities

PATH_IGNORE_REGEX:  re.Pattern = re.compile(r";[0-9]+$")
PATH_INVALID_REGEX: re.Pattern = re.compile(r"[^0-9A-Z_./]")

def normalizePath(path: str, isDirectory: bool = False) -> str:
	path = PATH_IGNORE_REGEX.sub("", path.upper())
	path = PATH_INVALID_REGEX.sub("_", path)

	if not path.startswith("/"):
		path = f"/{path}"

	if isDirectory:
		path = path.replace(".", "_")
	else:
		# The ISO9660 specification requires file names to always contain
		# exactly one period (even if no extension is present) and be terminated
		# with a version number.
		numPeriods: int = path.count(".")

		if numPeriods:
			path  = path.replace(".", "_", numPeriods - 1)
		else:
			path += "."

		path += ";1"

	return path

def restoreISOFileOrder(
	iso: PyCdlib, isoEntries: Iterable[DirectoryRecord | Inode],
	printLayout: bool = False
):
	# By default, when calling force_consistency(), pycdlib allocates files in
	# the same order as their respective directory records, which are in turn
	# sorted alphabetically as required by the ISO9660 specification. We're
	# going to "undo" this sorting by iterating through the provided array and
	# reassigning an LBA to each file and directory manually to restore the
	# intended order.
	iso.force_consistency()

	sectorLength: int = iso.logical_block_size
	sectorOffset: int = \
		iso.pvd.path_table_location_be + iso.pvd.path_table_num_extents

	if printLayout:
		sys.stderr.write("CD-ROM image layout:\n")

	# Allocate the indices (arrays of directory records) for the root directory
	# and each subfolder. Note that these will always be placed before any file
	# data, regardless of where they are listed in the configuration file.
	for entry in isoEntries:
		names:  list[str] = []
		length: int       = \
			(entry.data_length + sectorLength - 1) // sectorLength

		if isinstance(entry, DirectoryRecord):
			entry.set_data_location(sectorOffset, 0)
			names.append(entry.file_identifier().decode("ascii"))
		else:
			entry.set_extent_location(sectorOffset)

			for record, _ in entry.linked_records:
				if not isinstance(record, DirectoryRecord):
					continue

				record.set_data_location(sectorOffset, 0)
				names.append(record.file_identifier().decode("ascii"))

		if printLayout:
			nameList: str = ", ".join(names)

			sys.stderr.write(
				f"  [{sectorOffset:6d}-{sectorOffset + length - 1:6d}] "
				f"{nameList}\n"
			)

		sectorOffset += length

	if printLayout:
		sys.stderr.write(f"Total image size: {sectorOffset} sectors\n")

def showProgress(part: int, total: int, _: Any):
	if part >= total:
		sys.stderr.write("\rPacking finished.\n")
	else:
		sys.stderr.write(f"\rPacking: {part / total * 100.0:5.1f}%")

class PaddingFile(BinaryIO):
	mode: str = "rb"

	def read(self, length: int) -> bytes:
		return bytes(length)

## Main

def createParser() -> ArgumentParser:
	parser = ArgumentParser(
		description = \
			"Parses a JSON file containing a list of ISO9660 identifiers, file "
			"and directory entries and generates a CD-ROM image.",
		add_help    = False
	)

	group = parser.add_argument_group("Tool options")
	group.add_argument(
		"-h", "--help",
		action = "help",
		help   = "Show this help message and exit"
	)
	group.add_argument(
		"-q", "--quiet",
		action = "store_true",
		help   = "Suppress all non-error output"
	)

	group = parser.add_argument_group("CD-ROM image options")
	group.add_argument(
		"-l", "--level",
		type    = lambda value: int(value, 0),
		default = 1,
		help    = \
			"Set ISO9660 interchange level and maximum file name length "
			"(default 1)",
		metavar = "1-4"
	)
	group.add_argument(
		"-x", "--xa",
		action = "store_true",
		help   = "Add CD-XA header and metadata tags"
	)
	group.add_argument(
		"-S", "--system-area",
		type    = FileType("rb"),
		help    = \
			"Insert specified file (in 2048-bytes-per-sector format, up to 32 "
			"KB) into the image's system area",
		metavar = "file"
	)

	group = parser.add_argument_group("File paths")
	group.add_argument(
		"-s", "--source-dir",
		type    = Path,
		help    = \
			"Set path to directory containing source files (same directory as "
			"configuration file by default)",
		metavar = "dir"
	)
	group.add_argument(
		"configFile",
		type = FileType("rt"),
		help = "Path to JSON configuration file",
	)
	group.add_argument(
		"output",
		type = FileType("wb"),
		help = "Path to ISO9660 CD-ROM image to generate"
	)

	return parser

def main():
	parser: ArgumentParser = createParser()
	args:   Namespace      = parser.parse_args()

	with args.configFile as _file:
		configFile: dict[str, Any] = json.load(_file)
		sourceDir:  Path           = \
			args.source_dir or Path(_file.name).parent

	iso:         PyCdlib     = PyCdlib()
	paddingFile: PaddingFile = PaddingFile()

	identifiers: dict[str, str] = configFile.get("identifiers", {})

	iso.new(
		interchange_level  = args.level,
		sys_ident          = identifiers.get("system",        ""),
		vol_ident          = identifiers.get("volume",        ""),
		vol_set_ident      = identifiers.get("volumeSet",     ""),
		pub_ident_str      = identifiers.get("publisher",     ""),
		preparer_ident_str = identifiers.get("dataPreparer",  ""),
		app_ident_str      = identifiers.get("application",   ""),
		copyright_file     = identifiers.get("copyright",     ""),
		abstract_file      = identifiers.get("abstract",      ""),
		bibli_file         = identifiers.get("bibliographic", ""),
		joliet             = None,
		rock_ridge         = None,
		xa                 = args.xa,
		udf                = None
	)

	entryList:  list[dict[str, Any]]          = configFile["entries"]
	isoEntries: list[DirectoryRecord | Inode] = [
		iso.pvd.root_directory_record()
	]

	for entry in entryList:
		match entry.get("type", "file").strip():
			case "empty":
				name: str = normalizePath(entry["name"])

				iso.add_fp(
					fp       = paddingFile,
					length   = int(entry.get("size", 0)),
					iso_path = name
				)
				iso.set_hidden(iso_path = name)
				isoEntries.append(iso.inodes[-1])

			case "file":
				iso.add_file(
					filename = sourceDir / entry["source"],
					iso_path = normalizePath(entry["name"])
				)
				isoEntries.append(iso.inodes[-1])

			case "fileAlias":
				iso.add_hard_link(
					iso_old_path = normalizePath(entry["source"]),
					iso_new_path = normalizePath(entry["name"])
				)

			case "directoryAlias":
				iso.add_hard_link(
					iso_old_path = normalizePath(entry["source"], True),
					iso_new_path = normalizePath(entry["name"],   True)
				)

			case "directory":
				name: str = normalizePath(entry["name"], True)

				iso.add_directory(iso_path = name)
				isoEntries.append(iso.get_record(iso_path = name))

			case _type:
				raise KeyError(f"unsupported entry type '{_type}'")

	restoreISOFileOrder(iso, isoEntries, not args.quiet)

	with args.output as _file:
		iso.write_fp(
			outfp       = _file,
			progress_cb = None if args.quiet else showProgress
		)
		iso.close()

		if args.system_area:
			with args.system_area as inputFile:
				_file.seek(0)
				_file.write(inputFile.read(iso.logical_block_size * 16))

if __name__ == "__main__":
	main()
