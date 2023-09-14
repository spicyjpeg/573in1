
# Konami System 573 security cartridge tool

## Building

The following dependencies are required in order to build the project:

- CMake 3.25 or later;
- Python 3.10 or later;
- [Ninja](https://ninja-build.org/);
- a recent version of the GCC toolchain that targets the `mipsel-none-elf`
  architecture;
- optionally, `xorriso` and `chdman` (in order to build the CD-ROM image).

The toolchain can be installed on Windows, Linux or macOS by following the
instructions [here](https://github.com/grumpycoders/pcsx-redux/blob/main/src/mips/psyqo/GETTING_STARTED.md#the-toolchain)
and should be added to `PATH`. The other dependencies can be installed through a
package manager.

The Python script used to convert images at build time requires additional
dependencies which can be installed by running:

```
py -m pip install -r tools/requirements.txt   (Windows)
sudo pip install -r tools/requirements.txt    (Linux/macOS)
```

Once all prerequisites are installed, the tool can be built in debug mode (with
command-line argument parsing disabled and serial port logging enabled by
default) by running:

```
cmake --preset debug
cmake --build ./build
```

Replace `debug` with `release` to build in release mode or `min-size-release` to
optimize the executable for size. If `xorriso` is installed and listed in the
`PATH` environment variable, a bootable CD-ROM image will be generated alongside
the executable. A copy of the image in CHD format will additionally be generated
if MAME's `chdman` tool is also present.
