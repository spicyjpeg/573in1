
#pragma once

#include <stddef.h>
#include "common/gpu.hpp"
#include "common/util.hpp"
#include "main/cartdata.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"

/* Unlocked cartridge screens */

class CartActionsScreen : public ui::ListScreen {
protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void qrDump(ui::Context &ctx);
	void hddDump(ui::Context &ctx);
	void hexdump(ui::Context &ctx);
	void hddRestore(ui::Context &ctx);
	void reflash(ui::Context &ctx);
	void erase(ui::Context &ctx);
	void resetSystemID(ui::Context &ctx);
	void matchSystemID(ui::Context &ctx);
	void editSystemID(ui::Context &ctx);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class QRCodeScreen : public ui::ImageScreen {
public:
	bool valid;

	inline QRCodeScreen(void)
	: valid(false) {}

	inline bool generateCode(const char *textInput) {
		if (!gpu::generateQRCode(_image, 960, 256, textInput))
			return false;

		valid = true;
		return true;
	}
	inline bool generateCode(const uint8_t *binaryInput, size_t length) {
		if (!gpu::generateQRCode(_image, 960, 256, binaryInput, length))
			return false;

		valid = true;
		return true;
	}

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class HexdumpScreen : public ui::TextScreen {
private:
	char _bodyText[2048];

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class ReflashGameScreen : public ui::ListScreen {
protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class SystemIDEntryScreen : public ui::HexEntryScreen {
public:
	inline void getSystemID(cart::CartParser &parser) {
		parser.getIdentifiers()->systemID.copyTo(_buffer);
	}
	inline void setSystemID(cart::CartParser &parser) const {
		parser.getIdentifiers()->systemID.copyFrom(_buffer);
		parser.flush();
	}

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};
