
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "vendor/ff.h"
#include "vendor/miniz.h"
#include "vendor/qrcodegen.h"
#include "gpu.hpp"
#include "spu.hpp"
#include "util.hpp"

namespace asset {

/* Asset loader (wrapper around a zip file) */

class Asset {
public:
	void   *ptr;
	size_t length;

	inline Asset(void)
	: ptr(nullptr), length(0) {}
	inline ~Asset(void) {
		unload();
	}

	inline void unload(void) {
		if (ptr) {
			mz_free(ptr);
			ptr = nullptr;
		}
	}
};

class AssetLoader {
private:
	mz_zip_archive _zip;
	FIL            _fatFile;
	int            _hostFile;

public:
	bool     ready;
	uint32_t spuOffset;

	inline AssetLoader(uint32_t spuOffset = 0x1000)
	: _hostFile(-1), ready(false), spuOffset(spuOffset) {}
	inline ~AssetLoader(void) {
		close();
	}

	template<class T> inline size_t loadStruct(T &output, const char *path) {
		if (!mz_zip_reader_extract_file_to_mem(
			&_zip, path, &output, sizeof(T), 0
		))
			return 0;

		return sizeof(T);
	}

	bool openMemory(const void *zipData, size_t length);
	bool openFAT(const char *path);
	bool openHost(const char *path);
	void close(void);
	size_t loadAsset(Asset &output, const char *path);
	size_t loadTIM(gpu::Image &output, const char *path);
	size_t loadVAG(spu::Sound &output, const char *path);
};

/* String table manager */

static constexpr int TABLE_BUCKET_COUNT = 256;

struct [[gnu::packed]] StringTableEntry {
public:
	uint32_t hash;
	uint16_t offset, chained;
};

class StringTable : public Asset {
public:
	inline const char *operator[](util::Hash id) const {
		return get(id);
	}

	const char *get(util::Hash id) const;
	size_t format(char *buffer, size_t length, util::Hash id, ...) const;
};

/* QR code encoder */

bool generateQRCode(
	gpu::Image &output, int x, int y, const char *str,
	qrcodegen_Ecc ecc = qrcodegen_Ecc_MEDIUM
);
bool generateQRCode(
	gpu::Image &output, int x, int y, const uint8_t *data, size_t length,
	qrcodegen_Ecc ecc = qrcodegen_Ecc_MEDIUM
);

}
