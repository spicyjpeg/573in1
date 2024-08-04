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

#include <stdint.h>
#include "common/util/templates.hpp"
#include "common/spu.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"
#include "main/uimodals.hpp"

/* System information screens */

class IDEInfoScreen : public ui::TextScreen {
private:
	char _bodyText[2048];

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

/* Misc. screens */

class RTCTimeScreen : public ui::DateEntryScreen {
public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class LanguageScreen : public ui::ListScreen {
protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class ResolutionScreen : public ui::ListScreen {
protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class AboutScreen : public ui::TextScreen {
private:
	util::Data   _text;
	spu::Channel _loopChannel;

	util::Tween<uint16_t, util::QuadInEasing> _loopVolume;

public:
	void show(ui::Context &ctx, bool goBack = false);
	void hide(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};
