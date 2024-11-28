#!/bin/bash

ROOT_DIR="$(dirname "$0")"
ENCODE_VAG="psxavenc -q -t vag -a 64 -c 1"
ENCODE_MP3="lame -S -q 0 -b 96 -t --noreplaygain"

if ! which psxavenc lame >/dev/null 2>&1; then
	echo \
		"LAME and psxavenc (https://github.com/WonderfulToolchain/psxavenc)" \
		"must be installed and added to PATH in order to run this script."
	exit 1
fi

$ENCODE_VAG -f 22050 -L "$ROOT_DIR/about.wav"      "$ROOT_DIR/about.vag"
$ENCODE_VAG -f 22050    "$ROOT_DIR/alert.wav"      "$ROOT_DIR/alert.vag"
$ENCODE_VAG -f 22050    "$ROOT_DIR/click.wav"      "$ROOT_DIR/click.vag"
$ENCODE_VAG -f 22050    "$ROOT_DIR/enter.wav"      "$ROOT_DIR/enter.vag"
$ENCODE_VAG -f 22050    "$ROOT_DIR/exit.wav"       "$ROOT_DIR/exit.vag"
$ENCODE_VAG -f 22050    "$ROOT_DIR/move.wav"       "$ROOT_DIR/move.vag"
$ENCODE_VAG -f 33075    "$ROOT_DIR/screenshot.wav" "$ROOT_DIR/screenshot.vag"
$ENCODE_VAG -f 33075    "$ROOT_DIR/startup.wav"    "$ROOT_DIR/startup.vag"

# This file is used by the digital I/O board MP3 playback test.
$ENCODE_MP3 --resample 44100 "$ROOT_DIR/startup.wav" "$ROOT_DIR/startup.mp3"
