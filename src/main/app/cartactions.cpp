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

#include "common/util.hpp"
#include "main/app/cartactions.hpp"
#include "main/app/app.hpp"
#include "main/uibase.hpp"

/* Unlocked cartridge screens */

struct Action {
public:
	util::Hash name, prompt;
	void       (CartActionsScreen::*target)(ui::Context &ctx);
};

static constexpr int _NUM_SYSTEM_ID_ACTIONS = 3;

static const Action _ACTIONS[]{
	{
		.name   = "CartActionsScreen.qrDump.name"_h,
		.prompt = "CartActionsScreen.qrDump.prompt"_h,
		.target = &CartActionsScreen::qrDump
	}, {
		.name   = "CartActionsScreen.hddDump.name"_h,
		.prompt = "CartActionsScreen.hddDump.prompt"_h,
		.target = &CartActionsScreen::hddDump
	}, {
		.name   = "CartActionsScreen.hexdump.name"_h,
		.prompt = "CartActionsScreen.hexdump.prompt"_h,
		.target = &CartActionsScreen::hexdump
	}, {
		.name   = "CartActionsScreen.hddRestore.name"_h,
		.prompt = "CartActionsScreen.hddRestore.prompt"_h,
		.target = &CartActionsScreen::hddRestore
	}, {
		.name   = "CartActionsScreen.reflash.name"_h,
		.prompt = "CartActionsScreen.reflash.prompt"_h,
		.target = &CartActionsScreen::reflash
	}, {
		.name   = "CartActionsScreen.erase.name"_h,
		.prompt = "CartActionsScreen.erase.prompt"_h,
		.target = &CartActionsScreen::erase
	}, {
		.name   = "CartActionsScreen.resetSystemID.name"_h,
		.prompt = "CartActionsScreen.resetSystemID.prompt"_h,
		.target = &CartActionsScreen::resetSystemID
	}, {
		.name   = "CartActionsScreen.matchSystemID.name"_h,
		.prompt = "CartActionsScreen.matchSystemID.prompt"_h,
		.target = &CartActionsScreen::matchSystemID
	}, {
		.name   = "CartActionsScreen.editSystemID.name"_h,
		.prompt = "CartActionsScreen.editSystemID.prompt"_h,
		.target = &CartActionsScreen::editSystemID
	}
};

const char *CartActionsScreen::_getItemName(ui::Context &ctx, int index) const {
	return STRH(_ACTIONS[index].name);
}

void CartActionsScreen::qrDump(ui::Context &ctx) {
	if (APP->_qrCodeScreen.valid)
		ctx.show(APP->_qrCodeScreen, false, true);
	else
		APP->_runWorker(&App::_qrCodeWorker, APP->_qrCodeScreen, false, true);
}

void CartActionsScreen::hddDump(ui::Context &ctx) {
	APP->_messageScreen.previousScreens[MESSAGE_SUCCESS] =
		&(APP->_cartInfoScreen);
	APP->_messageScreen.previousScreens[MESSAGE_ERROR]   = this;

	APP->_runWorker(&App::_cartDumpWorker, APP->_messageScreen, false, true);
}

void CartActionsScreen::hexdump(ui::Context &ctx) {
	ctx.show(APP->_hexdumpScreen, false, true);
}

void CartActionsScreen::hddRestore(ui::Context &ctx) {
	APP->_filePickerScreen.previousScreen = this;
	APP->_filePickerScreen.setMessage(
		[](ui::Context &ctx) {
			ctx.show(APP->_confirmScreen, false, true);
		},
		STR("CartActionsScreen.hddRestore.filePrompt")
	);

	APP->_confirmScreen.previousScreen = &(APP->_fileBrowserScreen);
	APP->_confirmScreen.setMessage(
		[](ui::Context &ctx) {
			APP->_messageScreen.previousScreens[MESSAGE_ERROR] =
				&(APP->_fileBrowserScreen);

			APP->_runWorker(
				&App::_cartRestoreWorker, APP->_cartInfoScreen, true, true
			);
		},
		STR("CartActionsScreen.hddRestore.confirm")
	);

	APP->_filePickerScreen.reloadAndShow(ctx);
}

void CartActionsScreen::reflash(ui::Context &ctx) {
	ctx.show(APP->_reflashGameScreen, false, true);
}

void CartActionsScreen::erase(ui::Context &ctx) {
	APP->_confirmScreen.previousScreen = this;
	APP->_confirmScreen.setMessage(
		[](ui::Context &ctx) {
			APP->_messageScreen.previousScreens[MESSAGE_ERROR] =
				&(APP->_cartActionsScreen);

			APP->_runWorker(
				&App::_cartEraseWorker, APP->_cartInfoScreen, true, true
			);
		},
		STR("CartActionsScreen.erase.confirm")
	);

	ctx.show(APP->_confirmScreen, false, true);
}

void CartActionsScreen::resetSystemID(ui::Context &ctx) {
	if (!(APP->_cartParser->getIdentifiers()->systemID.isEmpty())) {
		APP->_confirmScreen.previousScreen = this;
		APP->_confirmScreen.setMessage(
			[](ui::Context &ctx) {
				util::clear(APP->_cartParser->getIdentifiers()->systemID);
				APP->_cartParser->flush();

				APP->_messageScreen.previousScreens[MESSAGE_ERROR] =
					&(APP->_cartActionsScreen);

				APP->_runWorker(
					&App::_cartWriteWorker, APP->_cartInfoScreen, true, true
				);
			},
			STR("CartActionsScreen.resetSystemID.confirm")
		);

		ctx.show(APP->_confirmScreen, false, true);
	} else {
		APP->_messageScreen.previousScreens[MESSAGE_ERROR] = this;
		APP->_messageScreen.setMessage(
			MESSAGE_ERROR, STR("CartActionsScreen.resetSystemID.error")
		);

		ctx.show(APP->_messageScreen, false, true);
	}
}

void CartActionsScreen::matchSystemID(ui::Context &ctx) {
	if (APP->_cartDump.flags & cart::DUMP_SYSTEM_ID_OK) {
		APP->_confirmScreen.previousScreen = this;
		APP->_confirmScreen.setMessage(
			[](ui::Context &ctx) {
				APP->_cartParser->getIdentifiers()->systemID.copyFrom(
					APP->_cartDump.systemID.data
				);
				APP->_cartParser->flush();

				APP->_messageScreen.previousScreens[MESSAGE_ERROR] =
					&(APP->_cartActionsScreen);

				APP->_runWorker(
					&App::_cartWriteWorker, APP->_cartInfoScreen, true, true
				);
			},
			STR("CartActionsScreen.matchSystemID.confirm")
		);

		ctx.show(APP->_confirmScreen, false, true);
	} else {
		APP->_messageScreen.previousScreens[MESSAGE_ERROR] = this;
		APP->_messageScreen.setMessage(
			MESSAGE_ERROR, STR("CartActionsScreen.matchSystemID.error")
		);

		ctx.show(APP->_messageScreen, false, true);
	}
}

void CartActionsScreen::editSystemID(ui::Context &ctx) {
	APP->_systemIDEntryScreen.getSystemID(*(APP->_cartParser));
	ctx.show(APP->_systemIDEntryScreen, false, true);
}

void CartActionsScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("CartActionsScreen.title");
	_prompt     = STRH(_ACTIONS[0].prompt);
	_itemPrompt = STR("CartActionsScreen.itemPrompt");

	_listLength = util::countOf(_ACTIONS) - _NUM_SYSTEM_ID_ACTIONS;

	if (APP->_cartParser) {
		if (APP->_cartParser->flags & cart::DATA_HAS_SYSTEM_ID)
			_listLength = util::countOf(_ACTIONS);
	}

	ListScreen::show(ctx, goBack);
}

void CartActionsScreen::update(ui::Context &ctx) {
	auto &action = _ACTIONS[_activeItem];
	_prompt      = STRH(action.prompt);

	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT))
			ctx.show(APP->_cartInfoScreen, true, true);
		else
			(this->*action.target)(ctx);
	}
}

static constexpr int _QR_CODE_SCALE   = 2;
static constexpr int _QR_CODE_PADDING = 6;

void QRCodeScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("QRCodeScreen.title");
	_prompt = STR("QRCodeScreen.prompt");

	_image         = &_code;
	_imageScale    = _QR_CODE_SCALE;
	_imagePadding  = _QR_CODE_SCALE * _QR_CODE_PADDING;
	_backdropColor = 0xffffff;

	ImageScreen::show(ctx, goBack);
}

void QRCodeScreen::update(ui::Context &ctx) {
	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_cartActionsScreen, true, true);
}

void HexdumpScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("HexdumpScreen.title");
	_body   = _bodyText;
	_prompt = STR("HexdumpScreen.prompt");

	size_t length = APP->_cartDump.getChipSize().dataLength;
	char *ptr = _bodyText, *end = &_bodyText[sizeof(_bodyText)];

	for (size_t i = 0; i < length; i += 16) {
		ptr += snprintf(ptr, end - ptr, "%04X: ", i);
		ptr += util::hexToString(ptr, &APP->_cartDump.data[i], 16, ' ');

		*(ptr++) = '\n';
	}

	*(--ptr) = 0;
	LOG_APP("%d buffer bytes free", end - ptr);

	TextScreen::show(ctx, goBack);
}

void HexdumpScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_cartActionsScreen, true, true);
}

const char *ReflashGameScreen::_getItemName(ui::Context &ctx, int index) const {
	static char name[96]; // TODO: get rid of this ugly crap

	APP->_cartDB.get(index)->getDisplayName(name, sizeof(name));
	return name;
}

void ReflashGameScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("ReflashGameScreen.title");
	_prompt     = STR("ReflashGameScreen.prompt");
	_itemPrompt = STR("ReflashGameScreen.itemPrompt");

	_listLength = APP->_cartDB.getNumEntries();

	ListScreen::show(ctx, goBack);
}

void ReflashGameScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT)) {
			ctx.show(APP->_cartActionsScreen, true, true);
		} else {
			APP->_confirmScreen.previousScreen = this;
			APP->_confirmScreen.setMessage(
				[](ui::Context &ctx) {
					APP->_messageScreen.previousScreens[MESSAGE_ERROR] =
						&(APP->_reflashGameScreen);

					APP->_runWorker(
						&App::_cartReflashWorker, APP->_cartInfoScreen, true,
						true
					);
				},
				STR("CartActionsScreen.reflash.confirm")
			);

			APP->_selectedEntry = APP->_cartDB.get(_activeItem);
			ctx.show(APP->_confirmScreen, false, true);
		}
	}
}

void SystemIDEntryScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("SystemIDEntryScreen.title");
	_body       = STR("SystemIDEntryScreen.body");
	_buttons[0] = STR("SystemIDEntryScreen.cancel");
	_buttons[1] = STR("SystemIDEntryScreen.ok");

	_numButtons   = 2;
	_bufferLength = 8;
	_separator    = '-';

	HexEntryScreen::show(ctx, goBack);
}

void SystemIDEntryScreen::update(ui::Context &ctx) {
	HexEntryScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (_activeButton == _buttonIndexOffset) {
			ctx.show(APP->_cartActionsScreen, true, true);
		} else if (_activeButton == (_buttonIndexOffset + 1)) {
			if (util::dsCRC8(_buffer, 7) == _buffer[7]) {
				APP->_confirmScreen.previousScreen = this;
				APP->_confirmScreen.setMessage(
					[](ui::Context &ctx) {
						APP->_systemIDEntryScreen.setSystemID(
							*(APP->_cartParser)
						);

						APP->_messageScreen.previousScreens[MESSAGE_ERROR] =
							&(APP->_systemIDEntryScreen);

						APP->_runWorker(
							&App::_cartWriteWorker, APP->_cartInfoScreen, true,
							true
						);
					},
					STR("CartActionsScreen.editSystemID.confirm")
				);

				ctx.show(APP->_confirmScreen, false, true);
			} else {
				APP->_messageScreen.previousScreens[MESSAGE_ERROR] = this;
				APP->_messageScreen.setMessage(
					MESSAGE_ERROR,
					STR("CartActionsScreen.editSystemID.error")
				);

				ctx.show(APP->_messageScreen, false, true);
			}
		}
	}
}
