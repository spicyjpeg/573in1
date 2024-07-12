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
#include "common/gpu.hpp"
#include "main/cart/cartdata.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"
#include "main/uimodals.hpp"

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
private:
	gpu::Image _code;

public:
	bool valid;

	inline QRCodeScreen(void)
	: valid(false) {}

	inline bool generateCode(const char *textInput) {
		if (!gpu::generateQRCode(_code, 960, 256, textInput))
			return false;

		valid = true;
		return true;
	}
	inline bool generateCode(const uint8_t *binaryInput, size_t length) {
		if (!gpu::generateQRCode(_code, 960, 256, binaryInput, length))
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
