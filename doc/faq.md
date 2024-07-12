
# FAQ and troubleshooting

## Running 573in1

### I cannot get past the BIOS "hardware error" screen and into the main menu

- Make sure the disc image has been burned correctly (and not by e.g. dragging
  the image file onto the disc). The disc should appear as `573IN1_xxxxx` and
  contain a bunch of files named `README.TXT`, `PSX.EXE` and so on.
- Try using a different disc and/or another CD-ROM drive. A list of drives known
  to be compatible with the stock System 573 BIOS can be found
  [here](https://psx-spx.consoledev.net/konamisystem573/#known-working-replacement-drives).
- Ensure the BIOS is not reporting an actual hardware error with the 573. A
  CD-ROM error will result in `CDR BAD` being displayed, while other errors will
  be reported under the PCB location of the chip that failed (e.g. `18E BAD` for
  a JVS MCU error). A list of BIOS errors can be found
  [here](https://psx-spx.consoledev.net/konamisystem573/#boot-sequence).

### I get an error about a missing file or the CD-ROM being incorrect

- You have booted into the game currently installed on the 573's internal flash
  rather than from the disc. Turn off DIP switch 4 (the rightmost one) and power
  cycle the 573.

## Security cartridges

### My cartridge does not get recognized

- Make sure the cartridge's contacts are clean and the security cartridge
  connector on the 573 has no bent pins and is not worn out, corroded or
  otherwise damaged.

## IDE devices

### My IDE cable does not fit into the 573

- The 573 motherboard has pin 20 populated on the IDE connector, but most cables
  use it as a keying pin (as per later versions of the IDE specification) and
  thus have it blocked off. You may either try to find another cable or drill a
  small hole in the cable's connector.
- You can also cut or desolder pin 20 on the 573, however this is not
  recommended.

### My IDE drive, CF card or adapter does not show up

- Try using a different drive or adapter. Some CF cards, SD card adapters and
  IDE-to-SATA converters are known to be problematic.

### My IDE drive, CF card or adapter shows up but is unreadable

- Ensure the drive supports LBA addressing. All CF cards, SD card adapters and
  the vast majority of hard drives do, however older hard drives (typically ones
  with a capacity lower than 8 GB or manufactured before the mid 1990s) may only
  support CHS addressing.
- Ensure the drive is formatted with a single FAT12, FAT16, FAT32 or exFAT
  partition. Other filesystems such as NTFS or ext4 are not supported.

## Miscellaneous

### How do I obtain more information about an error that happened?

- You may press the test button on your 573 or cabinet at any time to toggle the
  log window, which will contain detailed information about errors.
- If an IDE hard drive is connected you can also take screenshots by holding
  down the test button. Screenshots are saved to the `573in1` directory in the
  root of the filesystem.
