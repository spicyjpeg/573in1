#!/bin/bash

ROOT_DIR="$(pwd)"
PROJECT_DIR="$ROOT_DIR/cart-tool"
OPENBIOS_DIR="$ROOT_DIR/nugget/openbios"
TOOLCHAIN_DIR="$ROOT_DIR/gcc-mipsel-none-elf"

## Build project

cmake --preset release -DTOOLCHAIN_PATH="$TOOLCHAIN_DIR" "$PROJECT_DIR" \
	|| exit 1
cmake --build "$PROJECT_DIR/build" \
	|| exit 1

RELEASE_NAME="$(
	ls "$PROJECT_DIR/build" |
	grep -E -o '^cart-tool-[0-9]+\.[0-9]+\.[0-9]+' |
	head -n 1
)"

mkdir -p "$ROOT_DIR/$RELEASE_NAME"
cd "$ROOT_DIR/$RELEASE_NAME"
cp \
	"$PROJECT_DIR/build/$RELEASE_NAME.chd" \
	"$PROJECT_DIR/build/$RELEASE_NAME.iso" \
	"$PROJECT_DIR/build/$RELEASE_NAME.psexe" \
	"$PROJECT_DIR/build/readme.txt" \
	.

## Build BIOS ROM

cd "$OPENBIOS_DIR"

make \
	BUILD=Release \
	PREFIX="$TOOLCHAIN_DIR/bin/mipsel-none-elf" \
	FORMAT=elf32-littlemips \
	FASTBOOT=true \
	EMBED_PSEXE="$PROJECT_DIR/build/${RELEASE_NAME}-tiny.psexe" \
	|| exit 2

cd "$ROOT_DIR/$RELEASE_NAME"
cp "$OPENBIOS_DIR/openbios.bin" "${RELEASE_NAME}-bios.bin"

## Package release

zip -9 -r "$ROOT_DIR/$RELEASE_NAME.zip" . \
	|| exit 3

#cd "$ROOT_DIR"
#rm -rf "$RELEASE_NAME"
