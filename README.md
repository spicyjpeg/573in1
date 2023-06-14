
# Konami System 573 security cartridge tool

## Building

The following dependencies are required in order to build the project:

- CMake 3.25 or later;
- Python 3.10 or later;
- [Ninja](https://ninja-build.org/);
- a recent version of the GCC toolchain that targets the `mipsel-none-elf`
  architecture.

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
optimize the executable for size. If MAME's `chdman` tool is installed and
listed in the `PATH` environment variable, a CHD image will be generated in
addition to the raw CD-ROM image.
