/*
 * 573in1 - Copyright (C) 2022-2024 spicyjpeg
 *
 * 573in1 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * 573in1 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * 573in1. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/blkdev/device.hpp"
#include "common/util/misc.hpp"
#include "common/util/templates.hpp"

namespace fs {

static constexpr size_t MC_SECTOR_LENGTH       = 128;
static constexpr size_t MC_SECTORS_PER_CLUSTER = 64;
static constexpr size_t MC_MAX_CLUSTERS        = 15;
static constexpr size_t MC_MAX_RELOC_SECTORS   = 20;

static constexpr size_t MC_CLUSTER_LENGTH =
	MC_SECTOR_LENGTH * MC_SECTORS_PER_CLUSTER;

/* Memory card data structures */

enum MemoryCardBaseLBA : uint32_t {
	MC_LBA_HEADER        = 0x00,
	MC_LBA_RECORD_TABLE  = 0x01,
	MC_LBA_RELOC_TABLE   = 0x10,
	MC_LBA_RELOC_DATA    = 0x24,
	MC_LBA_UNIROM_CONFIG = 0x39,
	MC_LBA_NOCASH_CONFIG = 0x3e,
	MC_LBA_CLUSTER_DATA  = 0x40
};

class [[gnu::packed]] MemoryCardSector {
public:
	void updateChecksum(void);
	bool validateChecksum(void) const;
};

class [[gnu::packed]] MemoryCardHeader : public MemoryCardSector {
public:
	uint16_t magic;          // 0x00-0x01
	uint8_t  _reserved[125]; // 0x02-0x7e
	uint8_t  checksum;       // 0x7f

	inline bool validateMagic(void) const {
		return (magic == "MC"_c);
	}
};

enum MemoryCardRecordFlag : uint32_t {
	MC_RECORD_TYPE_BITMASK  = 15 << 0,
	MC_RECORD_TYPE_NONE     =  0 << 0,
	MC_RECORD_TYPE_FIRST    =  1 << 0,
	MC_RECORD_TYPE_MIDDLE   =  2 << 0,
	MC_RECORD_TYPE_LAST     =  3 << 0,
	MC_RECORD_STATE_BITMASK = 15 << 4,
	MC_RECORD_STATE_USED    =  5 << 4,
	MC_RECORD_STATE_FREE    = 10 << 4
};

class [[gnu::packed]] MemoryCardRecord : public MemoryCardSector {
public:
	uint32_t flags;         // 0x00-0x03
	uint32_t length;        // 0x04-0x07
	int16_t  chainIndex;    // 0x08-0x09
	char     name[21];      // 0x0a-0x1e
	uint8_t  _reserved[96]; // 0x1f-0x7e
	uint8_t  checksum;      // 0x7f

	inline bool isUsed(void) const {
		return ((flags & MC_RECORD_STATE_BITMASK) == MC_RECORD_STATE_USED);
	}
	inline bool isFirstCluster(void) const {
		return (flags == (MC_RECORD_TYPE_FIRST | MC_RECORD_STATE_USED));
	}

	void clear(void);
};

class [[gnu::packed]] MemoryCardRelocListEntry : public MemoryCardSector {
public:
	int32_t sector;         // 0x00-0x03
	uint8_t _reserved[123]; // 0x04-0x7e
	uint8_t checksum;       // 0x7f

	void init(int _sector);
};

/* Unirom and no$psx configuration structures */

enum UniromAutobootMode : uint8_t {
	UNIROM_AUTOBOOT_NONE        = 0,
	UNIROM_AUTOBOOT_CDROM       = 1,
	UNIROM_AUTOBOOT_CAETLA_FAST = 2,
	UNIROM_AUTOBOOT_CAETLA_FULL = 3
};

enum NocashAutobootMode : uint8_t {
	NOCASH_AUTOBOOT_NONE  = 0,
	NOCASH_AUTOBOOT_CDROM = 1
};

enum NocashAudioFlag : uint8_t {
	NOCASH_AUDIO_STEREO = 0 << 7,
	NOCASH_AUDIO_MONO   = 1 << 7
};

enum NocashMemoryCardSpeed : uint8_t {
	NOCASH_MC_SPEED_1X               = 0,
	NOCASH_MC_SPEED_2X               = 1,
	NOCASH_MC_SPEED_FAST             = 2,
	NOCASH_MC_SPEED_FAST_NO_CHECKSUM = 3
};

enum NocashVideoMode : uint8_t {
	NOCASH_VIDEO_AUTO = 0,
	NOCASH_VIDEO_NTSC = 1,
	NOCASH_VIDEO_PAL  = 2
};

enum NocashAnalogMode : uint8_t {
	NOCASH_ANALOG_OFF  = 0,
	NOCASH_ANALOG_ON   = 1,
	NOCASH_ANALOG_AUTO = 2
};

enum NocashMouseFlag : uint8_t {
	NOCASH_MOUSE_REMAP_SIO1    = 1 << 0,
	NOCASH_MOUSE_USE_THRESHOLD = 1 << 6,
	NOCASH_MOUSE_SWAP_BUTTONS  = 1 << 7
};

enum NocashTTYRedirectMode : uint8_t {
	NOCASH_TTY_AUTO       = 0,
	NOCASH_TTY_NONE       = 1,
	NOCASH_TTY_DEBUG_UART = 2,
	NOCASH_TTY_SIO1       = 3
};

class [[gnu::packed]] MemoryCardUniromConfig : public MemoryCardSector {
public:
	uint32_t           magic[2];       // 0x00-0x07
	uint8_t            version;        // 0x08
	UniromAutobootMode autoboot;       // 0x09
	uint8_t            _reserved[117]; // 0x0a-0x7e
	uint8_t            checksum;       // 0x7f

	inline bool validateMagic(void) const {
		return
			(magic[0] == "hors"_c) && (magic[1] == "ebag"_c) && (version == 1);
	}
};

class [[gnu::packed]] MemoryCardNocashConfig : public MemoryCardSector {
public:
	uint32_t              magic[2];        // 0x00-0x07
	uint8_t               version;         // 0x08
	NocashAutobootMode    autoboot;        // 0x09
	uint8_t               audioFlags;      // 0x0a
	NocashMemoryCardSpeed memoryCardSpeed; // 0x0b
	NocashVideoMode       videoMode;       // 0x0c
	int8_t                screenOffsetX;   // 0x0d
	int8_t                screenOffsetY;   // 0x0e
	NocashAnalogMode      analogMode;      // 0x0f
	uint8_t               mouseFlags;      // 0x10
	NocashTTYRedirectMode ttyRedirect;     // 0x11
	uint8_t               _reserved[45];   // 0x12-0x3e
	char                  cardLabel[32];   // 0x3f-0x5e
	uint8_t               _reserved2[32];  // 0x5f-0x7e
	uint8_t               checksum;        // 0x7f

	inline bool validateMagic(void) const {
		return
			(magic[0] == "<CON"_c) && (magic[1] == "FIG>"_c) && (version == 1);
	}
};

/* Sector I/O and relocation handler */

class MemoryCardIOHandler {
private:
	blkdev::Device             *_dev;
	util::MutexFlags<uint32_t> _mutex;

	uint16_t _relocations[MC_MAX_RELOC_SECTORS];

	bool _relocate(const void *data, uint32_t lba);
	bool _deleteRelocation(uint32_t lba);

public:
	inline MemoryCardIOHandler(void)
	: _dev(nullptr) {}

	inline bool readDirect(void *data, uint32_t lba) const {
		return !_dev->read(data, lba, 1);
	}
	inline bool writeDirect(const void *data, uint32_t lba) const {
		return !_dev->write(data, lba, 1);
	}

	bool init(blkdev::Device &dev);
	bool readRelocated(void *data, uint32_t lba);
	bool writeRelocated(const void *data, uint32_t lba);
};

}
