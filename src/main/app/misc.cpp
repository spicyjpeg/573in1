
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/file.hpp"
#include "common/ide.hpp"
#include "common/spu.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/app/misc.hpp"
#include "main/uibase.hpp"
#include "ps1/gpucmd.h"

/* System information screens */

static const util::Hash _IDE_HEADERS[]{
	"IDEInfoScreen.ide.header.primary"_h,
	"IDEInfoScreen.ide.header.secondary"_h
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

	// IDE drives
	for (int i = 0; i < 2; i++) {
		auto &dev = ide::devices[i];

		_PRINT(STRH(_IDE_HEADERS[i]));

		if (dev.flags & ide::DEVICE_READY) {
			_PRINT(
				STR("IDEInfoScreen.ide.commonInfo"), dev.model, dev.revision,
				dev.serialNumber
			);

			if (dev.flags & ide::DEVICE_ATAPI) {
				_PRINT(
					STR("IDEInfoScreen.ide.atapiInfo"),
					(dev.flags & ide::DEVICE_HAS_PACKET16) ? 16 : 12
				);
			} else {
				_PRINT(
					STR("IDEInfoScreen.ide.ataInfo"),
					uint64_t(dev.capacity / (0x100000 / ide::ATA_SECTOR_SIZE)),
					(dev.flags & ide::DEVICE_HAS_LBA48) ? 48 : 28
				);

				if (dev.flags & ide::DEVICE_HAS_TRIM)
					_PRINT(STR("IDEInfoScreen.ide.hasTrim"));
				if (dev.flags & ide::DEVICE_HAS_FLUSH)
					_PRINT(STR("IDEInfoScreen.ide.hasFlush"));
			}
		} else {
			_PRINT(STR("IDEInfoScreen.ide.error"));
		}

		_PRINTLN();
	}

	// FAT file system
	auto &fs    = APP->_fileProvider;
	auto fsType = fs.getFileSystemType();

	_PRINT(STR("IDEInfoScreen.fat.header"));

	if (fsType) {
		char label[32];
		auto serial = fs.getSerialNumber();

		fs.getVolumeLabel(label, sizeof(label));
		_PRINT(
			STR("IDEInfoScreen.fat.info"), _FAT_TYPES[fsType], label,
			serial >> 16, serial & 0xffff, fs.getCapacity() / 0x100000,
			fs.getFreeSpace() / 0x100000
		);
	} else {
		_PRINT(STR("IDEInfoScreen.fat.error"));
	}

	//*(--ptr) = 0;
	LOG("remaining=%d", end - ptr);

	TextScreen::show(ctx, goBack);
}

void IDEInfoScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_mainMenuScreen, true, true);
}

/* Misc. screens */

struct Resolution {
public:
	util::Hash name;
	int        width, height;
	bool       forceInterlace;
};

static const Resolution _RESOLUTIONS[]{
	{
		.name           = "ResolutionScreen.320x240p"_h,
		.width          = 320,
		.height         = 240,
		.forceInterlace = false
	}, {
		.name           = "ResolutionScreen.320x240i"_h,
		.width          = 320,
		.height         = 240,
		.forceInterlace = true
	}, {
		.name           = "ResolutionScreen.368x240p"_h,
		.width          = 368,
		.height         = 240,
		.forceInterlace = false
	}, {
		.name           = "ResolutionScreen.368x240i"_h,
		.width          = 368,
		.height         = 240,
		.forceInterlace = true
	}, {
		.name           = "ResolutionScreen.512x240p"_h,
		.width          = 512,
		.height         = 240,
		.forceInterlace = false
	}, {
		.name           = "ResolutionScreen.512x240i"_h,
		.width          = 512,
		.height         = 240,
		.forceInterlace = true
	}, {
		.name           = "ResolutionScreen.640x240p"_h,
		.width          = 640,
		.height         = 240,
		.forceInterlace = false
	}, {
		.name           = "ResolutionScreen.640x240i"_h,
		.width          = 640,
		.height         = 240,
		.forceInterlace = true
#if 0
	}, {
		.name           = "ResolutionScreen.640x480i"_h,
		.width          = 640,
		.height         = 480,
		.forceInterlace = true
#endif
	}
};

const char *ResolutionScreen::_getItemName(ui::Context &ctx, int index) const {
	return STRH(_RESOLUTIONS[index].name);
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
				GP1_MODE_NTSC, res.width, res.height, res.forceInterlace
			);

		ctx.show(APP->_mainMenuScreen, true, true);
	}
}

static constexpr uint16_t _LOOP_FADE_IN_VOLUME = spu::MAX_VOLUME / 2;
static constexpr int      _LOOP_FADE_IN_TIME   = 30;

void AboutScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("AboutScreen.title");
	_prompt = STR("AboutScreen.prompt");

	APP->_resourceProvider.loadData(_text, "assets/about.txt");

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
	_loopChannel = ctx.sounds[ui::SOUND_ABOUT_SCREEN].play(0);
}

void AboutScreen::hide(ui::Context &ctx, bool goBack) {
	_body = nullptr;
	_text.destroy();

	TextScreen::hide(ctx, goBack);
	spu::stopChannel(_loopChannel);
}

void AboutScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);
	spu::setChannelVolume(_loopChannel, _loopVolume.getValue(ctx.time));

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_mainMenuScreen, true, true);
}
