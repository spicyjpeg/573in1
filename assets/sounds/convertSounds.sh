#!/bin/bash

ROOT_DIR="$(dirname "$0")"
COMMAND="psxavenc -q -t vag -a 64 -c 1"

if ! which psxavenc >/dev/null 2>&1; then
	echo \
		"psxavenc (https://github.com/WonderfulToolchain/psxavenc) must be" \
		"built and added to PATH in order to run this script."
	exit 1
fi

$COMMAND -f 22050 -L "$ROOT_DIR/about.wav"      "$ROOT_DIR/about.vag"
$COMMAND -f 22050    "$ROOT_DIR/alert.wav"      "$ROOT_DIR/alert.vag"
$COMMAND -f 22050    "$ROOT_DIR/click.wav"      "$ROOT_DIR/click.vag"
$COMMAND -f 22050    "$ROOT_DIR/enter.wav"      "$ROOT_DIR/enter.vag"
$COMMAND -f 22050    "$ROOT_DIR/exit.wav"       "$ROOT_DIR/exit.vag"
$COMMAND -f 22050    "$ROOT_DIR/move.wav"       "$ROOT_DIR/move.vag"
$COMMAND -f 33075    "$ROOT_DIR/screenshot.wav" "$ROOT_DIR/screenshot.vag"
$COMMAND -f 33075    "$ROOT_DIR/startup.wav"    "$ROOT_DIR/startup.vag"
