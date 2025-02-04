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
#include <stdio.h>
#include "common/blkdev/device.hpp"
#include "common/fs/file.hpp"
#include "common/sys573/base.hpp"
#include "common/util/hash.hpp"
#include "common/util/templates.hpp"
#include "common/spu.hpp"
#include "main/app/app.hpp"
#include "main/app/misc.hpp"
#include "main/uibase.hpp"
#include "ps1/gpucmd.h"

/* System information screens */

struct IDEInfoHeader {
public:
	util::Hash device, fat, iso9660;
};

static const IDEInfoHeader _IDE_INFO_HEADERS[]{
	{
		.device  = "IDEInfoScreen.device.header.primary"_h,
		.fat     = "IDEInfoScreen.fat.header.primary"_h,
		.iso9660 = "IDEInfoScreen.iso9660.header.primary"_h
	}, {
		.device  = "IDEInfoScreen.device.header.secondary"_h,
		.fat     = "IDEInfoScreen.fat.header.secondary"_h,
		.iso9660 = "IDEInfoScreen.iso9660.header.secondary"_h
	}
};

static const char *const _FAT_TYPES[]{
	nullptr,
	"FAT12",
	"FAT16",
	"FAT32",
	"exFAT"
};

#define _PRINT(...) (ptr += snprintf(ptr, end - ptr __VA_OPT__(,) __VA_ARGS__))
#define _PRINTLN()  (*(ptr++) = '\n')

void IDEInfoScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("IDEInfoScreen.title");
	_body   = _bodyText;
	_prompt = STR("IDEInfoScreen.prompt");

	char *ptr = _bodyText, *end = &_bodyText[sizeof(_bodyText)];

	for (int i = 0; i < 2; i++) {
		auto &header = _IDE_INFO_HEADERS[i];
		auto mp      = APP->_fileIO.getMountPoint(IDE_MOUNT_POINTS[i]);

		// Device information
		auto dev = mp->dev;

		_PRINT(STRH(header.device));

		if (dev) {
			_PRINT(
				STR("IDEInfoScreen.device.commonInfo"),
				dev->model,
				dev->revision,
				dev->serialNumber
			);

			if (dev->type == blkdev::ATAPI) {
				_PRINT(
					STR("IDEInfoScreen.device.atapiInfo"),
					(dev->flags & blkdev::REQUIRES_EXT_PACKET) ? 16 : 12
				);
			} else {
				_PRINT(
					STR("IDEInfoScreen.device.ataInfo"),
					uint64_t(dev->capacity / (0x100000 / 512)),
					(dev->flags & blkdev::SUPPORTS_EXT_LBA) ? 48 : 28
				);

				if (dev->flags & blkdev::SUPPORTS_TRIM)
					_PRINT(STR("IDEInfoScreen.device.hasTrim"));
				if (dev->flags & blkdev::SUPPORTS_FLUSH)
					_PRINT(STR("IDEInfoScreen.device.hasFlush"));
			}
		} else {
			_PRINT(STR("IDEInfoScreen.device.error"));
		}

		_PRINTLN();

		// Filesystem information
		auto provider = mp->provider;

		if (!provider)
			continue;

		if (provider->type == fs::ISO9660) {
			_PRINT(STRH(header.iso9660));

			_PRINT(
				STR("IDEInfoScreen.iso9660.info"),
				provider->volumeLabel,
				provider->capacity / 0x100000
			);
		} else {
			_PRINT(STRH(header.fat));

			_PRINT(
				STR("IDEInfoScreen.fat.info"),
				_FAT_TYPES[provider->type],
				provider->volumeLabel,
				provider->serialNumber >> 16,
				provider->serialNumber & 0xffff,
				provider->capacity       / 0x100000,
				provider->getFreeSpace() / 0x100000
			);
		}

		_PRINTLN();
	}

	*(--ptr) = 0;
	LOG_APP("%d buffer bytes free", end - ptr);

	TextScreen::show(ctx, goBack);
}

void IDEInfoScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_mainMenuScreen, true, true);
}

/* Misc. screens */

void RTCTimeScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("RTCTimeScreen.title");
	_body       = STR("RTCTimeScreen.body");
	_buttons[0] = STR("RTCTimeScreen.cancel");
	_buttons[1] = STR("RTCTimeScreen.ok");

	_numButtons = 2;

	sys573::getRTCTime(_date);
	if (!_date.isValid())
		_date.reset();

	_date.second = 0;

	DateEntryScreen::show(ctx, goBack);
}

void RTCTimeScreen::update(ui::Context &ctx) {
	DateEntryScreen::update(ctx);

	if (
		ctx.buttons.pressed(ui::BTN_START) &&
		(_activeButton >= _buttonIndexOffset)
	) {
		if (_activeButton == (_buttonIndexOffset + 1))
			sys573::setRTCTime(_date);

		ctx.show(APP->_mainMenuScreen, true, true);
	}
}

struct Language {
public:
	util::Hash name;
	const char *path;
};

static const Language _LANGUAGES[]{
	{
		.name = "LanguageScreen.en"_h,
		.path = "res:/assets/lang/en.lang"
	}, {
		.name = "LanguageScreen.it"_h,
		.path = "res:/assets/lang/it.lang"
	}
};

const char *LanguageScreen::_getItemName(ui::Context &ctx, int index) const {
	return STRH(_LANGUAGES[index].name);
}

void LanguageScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("LanguageScreen.title");
	_prompt     = STR("LanguageScreen.prompt");
	_itemPrompt = STR("LanguageScreen.itemPrompt");

	_listLength = util::countOf(_LANGUAGES);

	ListScreen::show(ctx, goBack);
}

void LanguageScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (!ctx.buttons.held(ui::BTN_LEFT) && !ctx.buttons.held(ui::BTN_RIGHT))
			APP->_fileIO.loadData(
				APP->_stringTable,
				_LANGUAGES[_activeItem].path
			);

		ctx.show(APP->_mainMenuScreen, true, true);
	}
}

struct Resolution {
public:
	uint16_t width, height;
	bool     forceInterlace;
};

static const Resolution _RESOLUTIONS[]{
	{ .width = 320, .height = 240, .forceInterlace = false },
	{ .width = 320, .height = 240, .forceInterlace = true  },
	{ .width = 368, .height = 240, .forceInterlace = false },
	{ .width = 368, .height = 240, .forceInterlace = true  },
	{ .width = 512, .height = 240, .forceInterlace = false },
	{ .width = 512, .height = 240, .forceInterlace = true  },
	{ .width = 640, .height = 240, .forceInterlace = false },
	{ .width = 640, .height = 240, .forceInterlace = true  },
	{ .width = 640, .height = 480, .forceInterlace = true  }
};

const char *ResolutionScreen::_getItemName(ui::Context &ctx, int index) const {
	static char name[96]; // TODO: get rid of this ugly crap

	auto       &res = _RESOLUTIONS[index];
	util::Hash format;

	if (res.height > 240)
		format = "ResolutionScreen.interlaced480"_h;
	else if (res.forceInterlace)
		format = "ResolutionScreen.interlaced"_h;
	else
		format = "ResolutionScreen.progressive"_h;

	snprintf(name, sizeof(name), STRH(format), res.width, res.height);
	return name;
}

void ResolutionScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("ResolutionScreen.title");
	_prompt     = STR("ResolutionScreen.prompt");
	_itemPrompt = STR("ResolutionScreen.itemPrompt");

	_listLength = util::countOf(_RESOLUTIONS);

	ListScreen::show(ctx, goBack);
}

void ResolutionScreen::update(ui::Context &ctx) {
	auto &res = _RESOLUTIONS[_activeItem];

	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (!ctx.buttons.held(ui::BTN_LEFT) && !ctx.buttons.held(ui::BTN_RIGHT))
			ctx.gpuCtx.setResolution(
				GP1_MODE_NTSC,
				res.width,
				res.height,
				res.forceInterlace
			);

		ctx.show(APP->_mainMenuScreen, true, true);
	}
}

static constexpr int _LOOP_FADE_IN_VOLUME = spu::MAX_VOLUME / 2;
static constexpr int _LOOP_FADE_IN_TIME   = 30;

void AboutScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("AboutScreen.title");
	_prompt = STR("AboutScreen.prompt");

	APP->_fileIO.loadData(_text, "res:/assets/about.txt");

	auto ptr = reinterpret_cast<char *>(_text.ptr);
	_body    = ptr;

	// Replace single newlines with spaces to reflow the text, unless the line
	// preceding the newline ends with a space. The last character is also cut
	// off and replaced with a null terminator.
	for (size_t i = _text.length - 1; i; i--, ptr++) {
		if (*ptr != '\n')
			continue;
		if (__builtin_isspace(ptr[-1]))
			continue;

		if (ptr[1] == '\n')
			i--, ptr++;
		else
			*ptr = ' ';
	}

	*ptr = 0;

	TextScreen::show(ctx, goBack);

	_loopVolume.setValue(
		ctx.time, 0, _LOOP_FADE_IN_VOLUME,
		ctx.gpuCtx.refreshRate * _LOOP_FADE_IN_TIME
	);
	_loopChannel = ctx.sounds[ui::SOUND_ABOUT_SCREEN].play(0, 0);
}

void AboutScreen::hide(ui::Context &ctx, bool goBack) {
	_body = nullptr;
	_text.destroy();

	TextScreen::hide(ctx, goBack);
	spu::stopChannel(_loopChannel);
}

void AboutScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	auto volume = _loopVolume.getValue(ctx.time);
	spu::setChannelVolume(_loopChannel, volume, volume);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_mainMenuScreen, true, true);
}
