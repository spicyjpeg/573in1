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

#include <stddef.h>
#include <stdint.h>
#include "common/fs/memorycard.hpp"
#include "common/storage/device.hpp"
#include "common/util/log.hpp"
#include "common/util/misc.hpp"
#include "common/util/templates.hpp"

namespace fs {

static constexpr int _MUTEX_TIMEOUT = 30000000;

/* Utilities */

void MemoryCardSector::updateChecksum(void) {
	auto ptr = reinterpret_cast<uint8_t *>(this) + 0x7f;

	*ptr = util::bitwiseXOR(
		reinterpret_cast<const uint8_t *>(this), sizeof(MemoryCardSector)
	);
}

bool MemoryCardSector::validateChecksum(void) const {
	auto ptr = reinterpret_cast<const uint8_t *>(this) + 0x7f;

	return (util::bitwiseXOR(
		reinterpret_cast<const uint8_t *>(this), sizeof(MemoryCardSector)
	) == *ptr);
}

void MemoryCardRecord::clear(void) {
	util::clear(*this);

	flags      = MC_RECORD_TYPE_NONE | MC_RECORD_STATE_FREE;
	chainIndex = -1;
	updateChecksum();
}

void MemoryCardRelocListEntry::init(int _sector) {
	util::clear(*this);

	sector = _sector;
	updateChecksum();
}

/* Sector I/O and relocation handler */

/*
 * The PS1 memory card filesystem supports a very crude and broken form of
 * overprovisioning. If write errors occur up to 20 sectors belonging to any
 * record may be relocated to a reserved area in the header, with a separate
 * 20-sector region being used as an index to (inefficiently) keep track of
 * which LBAs have been moved.
 *
 * While in theory sectors belonging to the header's directory area could also
 * be relocated, the PS1 kernel does not support moving non-file data and will
 * skip the relocation table entirely when accessing any header sector (see
 * https://github.com/grumpycoders/pcsx-redux/blob/main/src/mips/openbios/card/device.c).
 */

bool MemoryCardIOHandler::_relocate(const void *data, uint32_t lba) {
	for (size_t i = 0; i < MC_MAX_RELOC_SECTORS; i++) {
		if (_relocations[i])
			continue;

		MemoryCardRelocListEntry entry;

		entry.init(lba);

		// Attempt to relocate the sector. If the write succeeds, update the
		// relocation table accordingly.
		auto error = _dev->write(data, MC_LBA_RELOC_DATA + i, 1);

		if (error == storage::DRIVE_ERROR) {
			LOG_FS("write error, lba=0x%x, reloc=%d", lba, i);
			continue;
		}
		if (error)
			return false;

		error = _dev->write(&entry, MC_LBA_RELOC_TABLE + i, 1);

		if (error == storage::DRIVE_ERROR) {
			LOG_FS("write error, lba=0x%x, reloc=%d", lba, i);
			continue;
		}
		if (error)
			return false;

		_relocations[i] = lba;
		LOG_FS("lba=0x%x, reloc=%d", lba, i);
		return true;
	}

	LOG_FS("no spare sectors available");
	return false;
}

bool MemoryCardIOHandler::_deleteRelocation(uint32_t lba) {
	for (size_t i = 0; i < MC_MAX_RELOC_SECTORS; i++) {
		if (_relocations[i] != lba)
			continue;

		MemoryCardRelocListEntry entry;

		entry.init(lba);

		if (_dev->write(&entry, MC_LBA_RELOC_TABLE + i, 1)) {
			LOG_FS("write error, lba=0x%x, reloc=%d", lba, i);
			return false;
		}

		_relocations[lba] = 0;
		LOG_FS("lba=0x%x, reloc=%d", lba, i);
		break;
	}

	return true;
}

bool MemoryCardIOHandler::init(storage::Device &dev) {
	MemoryCardRelocListEntry entries[MC_MAX_RELOC_SECTORS];

	if (dev.read(entries, MC_LBA_RELOC_TABLE, MC_MAX_RELOC_SECTORS)) {
		LOG_FS("relocation table read failed");
		return false;
	}

	{
		util::MutexLock lock(_mutex, uint32_t(1), _MUTEX_TIMEOUT);

		if (!lock.locked) {
			LOG_FS("relocation mutex timeout");
			return false;
		}

		for (size_t i = 0; i < MC_MAX_RELOC_SECTORS; i++) {
			auto &entry = entries[i];

			if ((entry.sector >= 0) && entry.validateChecksum())
				_relocations[i] = entry.sector;
			else
				_relocations[i] = 0;
		}
	}

	_dev = &dev;
	return true;
}

bool MemoryCardIOHandler::readRelocated(void *data, uint32_t lba) {
	auto error = _dev->read(data, lba, 1);

	if (!error)
		return true;
	if (error != storage::DRIVE_ERROR)
		return false;

	{
		util::MutexLock lock(_mutex, uint32_t(1), _MUTEX_TIMEOUT);

		if (!lock.locked) {
			LOG_FS("relocation mutex timeout");
			return false;
		}

		for (size_t i = 0; i < MC_MAX_RELOC_SECTORS; i++) {
			if (_relocations[i] != lba)
				continue;
			if (!_dev->read(data, MC_LBA_RELOC_DATA + i, 1))
				return true;

			LOG_FS("read error, lba=0x%x, reloc=%d", lba, i);
			return false;
		}
	}

	LOG_FS("read error lba=0x%x, no reloc", lba);
	return false;
}

bool MemoryCardIOHandler::writeRelocated(const void *data, uint32_t lba) {
	// Always try to write to the original sector first, even if it has been
	// relocated before.
	auto error = _dev->write(data, lba, 1);

	if (!error)
		return _deleteRelocation(lba);
	if (error != storage::DRIVE_ERROR)
		return false;

	// If that fails, search for any existing relocation and attempt to
	// overwrite it. If the write in turn fails, or if no match is found,
	// relocate the sector to a spare one as a last resort.
	{
		util::MutexLock lock(_mutex, uint32_t(1), _MUTEX_TIMEOUT);

		if (!lock.locked) {
			LOG_FS("relocation mutex timeout");
			return false;
		}

		for (size_t i = 0; i < MC_MAX_RELOC_SECTORS; i++) {
			if (_relocations[i] != lba)
				continue;

			error = _dev->write(data, MC_LBA_RELOC_DATA + i, 1);

			if (!error)
				return true;
			if (error != storage::DRIVE_ERROR)
				return false;

			LOG_FS("write error, lba=0x%x, reloc=%d", lba, i);

			if (!_deleteRelocation(lba))
				return false;

			break;
		}

		return _relocate(data, lba);
	}
}

}
