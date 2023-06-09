
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "ps1/gpucmd.h"
#include "ps1/pcdrv.h"
#include "vendor/ff.h"
#include "vendor/miniz.h"
#include "vendor/qrcodegen.h"
#include "asset.hpp"

namespace asset {

/* Asset loader */

static constexpr uint32_t _ZIP_FLAGS =
	MZ_ZIP_FLAG_CASE_SENSITIVE | MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY;

bool AssetLoader::openMemory(const void *zipData, size_t length) {
	//close();
	mz_zip_zero_struct(&_zip);

	// Sorting the central directory in a zip with a small number of files is
	// just a waste of time.
	if (!mz_zip_reader_init_mem(&_zip, zipData, length, _ZIP_FLAGS)) {
		LOG("zip init error, code=%d", mz_zip_get_last_error(&_zip));
		return false;
	}

	LOG("ptr=0x%08x, length=0x%x", zipData, length);
	ready = true;
	return true;
}

bool AssetLoader::openFAT(const char *path) {
	auto error = f_open(&_fatFile, path, FA_READ | FA_OPEN_EXISTING);
	if (error) {
		LOG("FAT zip open error, code=%d", error);
		return false;
	}

	size_t fileSize = f_size(&_fatFile);

	_zip.m_pIO_opaque       = &_fatFile;
	_zip.m_pNeeds_keepalive = nullptr;
	_zip.m_pRead            = [](
		void *opaque, uint64_t offset, void *data, size_t length
	) -> size_t {
		auto fatFile = reinterpret_cast<FIL *>(opaque);

		auto error = f_lseek(fatFile, offset);
		if (error) {
			LOG("FAT zip seek error, code=%d", error);
			return 0;
		}

		size_t actualLength;

		error = f_read(fatFile, data, length, &actualLength);
		if (error) {
			LOG("FAT zip read error, code=%d", error);
			return 0;
		}

		return actualLength;
	};

	if (!mz_zip_reader_init(&_zip, fileSize, _ZIP_FLAGS)) {
		LOG("FAT zip init error, code=%d", mz_zip_get_last_error(&_zip));
		return false;
	}

	LOG("length=0x%x", fileSize);
	ready = true;
	return true;
}

bool AssetLoader::openHost(const char *path) {
	if (pcdrvInit() < 0)
		return false;

	_hostFile = pcdrvOpen(path, PCDRV_MODE_READ);
	if (_hostFile < 0) {
		LOG("host zip open error, code=%d", _hostFile);
		return false;
	}

	int fileSize = pcdrvSeek(_hostFile, 0, PCDRV_SEEK_END);
	if (fileSize < 0)
		return false;

	_zip.m_pIO_opaque       = reinterpret_cast<void *>(_hostFile);
	_zip.m_pNeeds_keepalive = nullptr;
	_zip.m_pRead            = [](
		void *opaque, uint64_t offset, void *data, size_t length
	) -> size_t {
		int hostFile = reinterpret_cast<int>(opaque);

		if (
			pcdrvSeek(hostFile, static_cast<uint32_t>(offset), PCDRV_SEEK_SET)
			!= int(offset)
		)
			return 0;

		int actualLength = pcdrvRead(hostFile, data, length);
		if (actualLength < 0)
			return 0;

		return actualLength;
	};

	if (!mz_zip_reader_init(&_zip, fileSize, _ZIP_FLAGS)) {
		LOG("host zip init error, code=%d", mz_zip_get_last_error(&_zip));
		return false;
	}

	LOG("length=0x%x", fileSize);
	ready = true;
	return true;
}

void AssetLoader::close(void) {
	if (!ready)
		return;
	if (_hostFile >= 0)
		pcdrvClose(_hostFile);
	else
		f_close(&_fatFile);

	mz_zip_reader_end(&_zip);
	ready = false;
}

size_t AssetLoader::loadAsset(Asset &output, const char *path) {
	output.ptr = mz_zip_reader_extract_file_to_heap(&_zip, path, &output.length, 0);

	if (!output.ptr)
		return 0;

	return output.length;
}

size_t AssetLoader::loadTIM(gpu::Image &output, const char *path) {
	size_t size;
	void   *data = mz_zip_reader_extract_file_to_heap(&_zip, path, &size, 0);

	if (!data)
		return 0;

	auto header = reinterpret_cast<const gpu::TIMHeader *>(data);
	auto ptr    = reinterpret_cast<const uint8_t *>(&header[1]);

	if (!output.initFromTIMHeader(header)) {
		mz_free(data);
		return 0;
	}
	if (header->flags & (1 << 3)) {
		auto clut = reinterpret_cast<const gpu::TIMSectionHeader *>(ptr);

		gpu::upload(clut->vram, &clut[1], true);
		ptr += clut->length;
	}

	auto image = reinterpret_cast<const gpu::TIMSectionHeader *>(ptr);

	gpu::upload(image->vram, &image[1], true);
	mz_free(data);
	return size;
}

size_t AssetLoader::loadVAG(spu::Sound &output, const char *path) {
	// Sounds should be decompressed and uploaded to the SPU one chunk at a
	// time, but whatever.
	size_t size;
	void   *data = mz_zip_reader_extract_file_to_heap(&_zip, path, &size, 0);

	if (!data)
		return 0;

	auto header = reinterpret_cast<const spu::VAGHeader *>(data);

	if (!output.initFromVAGHeader(header, spuOffset)) {
		mz_free(data);
		return 0;
	}

	spuOffset += spu::upload(
		spuOffset, reinterpret_cast<const uint32_t *>(&header[1]),
		size - sizeof(spu::VAGHeader), true
	);
	mz_free(data);
	return size;
}

/* String table manager */

const char *StringTable::get(util::Hash id) const {
	if (!ptr)
		return "missingno";

	auto blob  = reinterpret_cast<const char *>(ptr);
	auto table = reinterpret_cast<const StringTableEntry *>(ptr);

	auto entry = &table[id % TABLE_BUCKET_COUNT];

	if (entry->hash == id)
		return &blob[entry->offset];

	while (entry->chained) {
		entry = &table[entry->chained];

		if (entry->hash == id)
			return &blob[entry->offset];
	}

	return "missingno";
}

size_t StringTable::format(
	char *buffer, size_t length, util::Hash id, ...
) const {
	va_list ap;

	va_start(ap, id);
	size_t outLength = vsnprintf(buffer, length, get(id), ap);
	va_end(ap);

	return outLength;
}

/* QR code encoder */

static void _loadQRCode(
	gpu::Image &output, int x, int y, const uint32_t *qrCode
) {
	int size = qrcodegen_getSize(qrCode);
	gpu::RectWH rect;

	// Generate a 16-color (only 2 colors used) palette and place it below the
	// QR code in VRAM.
	const uint32_t palette[8]{ 0x8000ffff };

	rect.x = x;
	rect.y = y + size;
	rect.w = 16;
	rect.h = 1;
	gpu::upload(rect, palette, true);

	rect.y = y;
	rect.w = qrcodegen_getStride(qrCode) * 2;
	rect.h = size;
	gpu::upload(rect, &qrCode[1], true);

	output.initFromVRAMRect(rect, GP0_COLOR_4BPP);
	output.width   = size;
	output.palette = gp0_clut(x / 16, y + size);

	LOG("loaded at (%d,%d), size=%d", x, y, size);
}

bool generateQRCode(
	gpu::Image &output, int x, int y, const char *str, qrcodegen_Ecc ecc
) {
	uint32_t qrCode[qrcodegen_BUFFER_LEN_MAX];
	uint32_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

	auto segment = qrcodegen_makeAlphanumeric(
		str, reinterpret_cast<uint8_t *>(tempBuffer)
	);
	if (!qrcodegen_encodeSegments(&segment, 1, ecc, tempBuffer, qrCode)) {
		LOG("QR encoding failed");
		return false;
	}

	_loadQRCode(output, x, y, qrCode);
	return true;
}

bool generateQRCode(
	gpu::Image &output, int x, int y, const uint8_t *data, size_t length,
	qrcodegen_Ecc ecc
) {
	uint32_t qrCode[qrcodegen_BUFFER_LEN_MAX];
	uint32_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

	auto segment = qrcodegen_makeBytes(
		data, length, reinterpret_cast<uint8_t *>(tempBuffer)
	);
	if (!qrcodegen_encodeSegments(&segment, 1, ecc, tempBuffer, qrCode)) {
		LOG("QR encoding failed");
		return false;
	}

	_loadQRCode(output, x, y, qrCode);
	return true;
}

}
