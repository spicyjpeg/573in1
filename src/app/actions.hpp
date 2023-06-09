
#pragma once

#include <stddef.h>
#include "asset.hpp"
#include "uibase.hpp"
#include "uicommon.hpp"
#include "util.hpp"

/* Unlocked cartridge screens */

class CartActionsScreen : public ui::ListScreen {
protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void qrDump(ui::Context &ctx);
	void hexdump(ui::Context &ctx);
	void reflash(ui::Context &ctx);
	void erase(ui::Context &ctx);
	void resetSystemID(ui::Context &ctx);
	void editSystemID(ui::Context &ctx);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class QRCodeScreen : public ui::ImageScreen {
public:
	inline bool generateCode(const char *textInput) {
		return asset::generateQRCode(_image, 960, 128, textInput);
	}
	inline bool generateCode(const uint8_t *binaryInput, size_t length) {
		return asset::generateQRCode(_image, 960, 128, binaryInput, length);
	}

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};
