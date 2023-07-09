
#include "app/cartactions.hpp"
#include "app/app.hpp"
#include "defs.hpp"
#include "uibase.hpp"
#include "util.hpp"

/* Unlocked cartridge screens */

struct Action {
public:
	util::Hash name, prompt;
	void       (CartActionsScreen::*target)(ui::Context &ctx);
};

static constexpr int _NUM_SYSTEM_ID_ACTIONS    = 8;
static constexpr int _NUM_NO_SYSTEM_ID_ACTIONS = 5;

static const Action _ACTIONS[_NUM_SYSTEM_ID_ACTIONS]{
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
	APP->_setupWorker(&App::_qrCodeWorker);
	ctx.show(APP->_workerStatusScreen, false, true);
}

void CartActionsScreen::hddDump(ui::Context &ctx) {
	APP->_setupWorker(&App::_cartDumpWorker);
	ctx.show(APP->_workerStatusScreen, false, true);
}

void CartActionsScreen::hexdump(ui::Context &ctx) {
	ctx.show(APP->_hexdumpScreen, false, true);
}

void CartActionsScreen::reflash(ui::Context &ctx) {
	ctx.show(APP->_reflashGameScreen, false, true);
}

void CartActionsScreen::erase(ui::Context &ctx) {
	APP->_confirmScreen.setMessage(
		*this,
		[](ui::Context &ctx) {
			APP->_setupWorker(&App::_cartEraseWorker);
			ctx.show(APP->_workerStatusScreen, false, true);
		},
		STR("CartActionsScreen.erase.confirm")
	);

	ctx.show(APP->_confirmScreen, false, true);
}

void CartActionsScreen::resetSystemID(ui::Context &ctx) {
	if (!(APP->_parser->getIdentifiers()->systemID.isEmpty())) {
		APP->_confirmScreen.setMessage(
			*this,
			[](ui::Context &ctx) {
				APP->_parser->getIdentifiers()->systemID.clear();
				APP->_parser->flush();

				APP->_setupWorker(&App::_cartWriteWorker);
				ctx.show(APP->_workerStatusScreen, false, true);
			},
			STR("CartActionsScreen.resetSystemID.confirm")
		);

		ctx.show(APP->_confirmScreen, false, true);
	} else {
		APP->_errorScreen.setMessage(
			*this, STR("CartActionsScreen.resetSystemID.error")
		);

		ctx.show(APP->_errorScreen, false, true);
	}
}

void CartActionsScreen::matchSystemID(ui::Context &ctx) {
	if (APP->_dump.flags & cart::DUMP_SYSTEM_ID_OK) {
		APP->_confirmScreen.setMessage(
			*this,
			[](ui::Context &ctx) {
				APP->_parser->getIdentifiers()->systemID.copyFrom(
					APP->_dump.systemID.data
				);
				APP->_parser->flush();

				APP->_setupWorker(&App::_cartWriteWorker);
				ctx.show(APP->_workerStatusScreen, false, true);
			},
			STR("CartActionsScreen.matchSystemID.confirm")
		);

		ctx.show(APP->_confirmScreen, false, true);
	} else {
		APP->_errorScreen.setMessage(
			*this, STR("CartActionsScreen.matchSystemID.error")
		);

		ctx.show(APP->_errorScreen, false, true);
	}
}

void CartActionsScreen::editSystemID(ui::Context &ctx) {
	APP->_confirmScreen.setMessage(
		APP->_systemIDEntryScreen,
		[](ui::Context &ctx) {
			APP->_systemIDEntryScreen.setSystemID(*(APP->_parser));

			APP->_setupWorker(&App::_cartWriteWorker);
			ctx.show(APP->_workerStatusScreen, false, true);
		},
		STR("CartActionsScreen.editSystemID.confirm")
	);

	APP->_errorScreen.setMessage(
		APP->_systemIDEntryScreen, STR("CartActionsScreen.editSystemID.error")
	);

	ctx.show(APP->_systemIDEntryScreen, false, true);
}

void CartActionsScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("CartActionsScreen.title");
	_prompt     = STRH(_ACTIONS[0].prompt);
	_itemPrompt = STR("CartActionsScreen.itemPrompt");

	_listLength = _NUM_NO_SYSTEM_ID_ACTIONS;

	if (APP->_parser) {
		if (APP->_parser->flags & cart::DATA_HAS_SYSTEM_ID)
			_listLength = _NUM_SYSTEM_ID_ACTIONS;
	}

	ListScreen::show(ctx, goBack);
}

void CartActionsScreen::update(ui::Context &ctx) {
	auto &action = _ACTIONS[_activeItem];
	_prompt      = STRH(action.prompt);

	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		(this->*action.target)(ctx);
	if (
		(ctx.buttons.held(ui::BTN_LEFT) && ctx.buttons.pressed(ui::BTN_RIGHT)) ||
		(ctx.buttons.pressed(ui::BTN_LEFT) && ctx.buttons.held(ui::BTN_RIGHT))
	)
		ctx.show(APP->_cartInfoScreen, true, true);
}

static constexpr int _QR_CODE_SCALE   = 2;
static constexpr int _QR_CODE_PADDING = 6;

void QRCodeScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("QRCodeScreen.title");
	_prompt = STR("QRCodeScreen.prompt");

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

	size_t length = APP->_dump.getChipSize().dataLength;
	char *ptr = _bodyText, *end = &_bodyText[sizeof(_bodyText)];

	for (size_t i = 0; i < length; i += 16) {
		ptr += snprintf(ptr, end - ptr, "%04X: ", i);
		ptr += util::hexToString(ptr, &APP->_dump.data[i], 16, ' ');

		*(ptr++) = '\n';
	}

	*(--ptr) = 0;

	TextScreen::show(ctx, goBack);
}

void HexdumpScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_cartActionsScreen, true, true);
}

const char *ReflashGameScreen::_getItemName(ui::Context &ctx, int index) const {
	static char name[96]; // TODO: get rid of this ugly crap

	APP->_db.get(index)->getDisplayName(name, sizeof(name));
	return name;
}

void ReflashGameScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("ReflashGameScreen.title");
	_prompt     = STR("ReflashGameScreen.prompt");
	_itemPrompt = STR("ReflashGameScreen.itemPrompt");

	_listLength = APP->_db.getNumEntries();

	ListScreen::show(ctx, goBack);
}

void ReflashGameScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		APP->_confirmScreen.setMessage(
			*this,
			[](ui::Context &ctx) {
				APP->_setupWorker(&App::_cartReflashWorker);
				ctx.show(APP->_workerStatusScreen, false, true);
			},
			STR("CartActionsScreen.reflash.confirm")
		);

		APP->_selectedEntry = APP->_db.get(_activeItem);
		ctx.show(APP->_confirmScreen, false, true);
	} else if (
		(ctx.buttons.held(ui::BTN_LEFT) && ctx.buttons.pressed(ui::BTN_RIGHT)) ||
		(ctx.buttons.pressed(ui::BTN_LEFT) && ctx.buttons.held(ui::BTN_RIGHT))
	) {
		ctx.show(APP->_cartActionsScreen, true, true);
	}
}

void SystemIDEntryScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("SystemIDEntryScreen.title");
	_body       = STR("SystemIDEntryScreen.body");
	_buttons[0] = STR("SystemIDEntryScreen.cancel");
	_buttons[1] = STR("SystemIDEntryScreen.ok");

	_numButtons = 2;
	_locked     = false;

	_bufferLength = 8;
	_separator    = '-';

	HexEntryScreen::show(ctx, goBack);

	APP->_parser->getIdentifiers()->systemID.copyTo(_buffer);
}

void SystemIDEntryScreen::update(ui::Context &ctx) {
	HexEntryScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (_activeButton == _buttonIndexOffset) {
			ctx.show(APP->_cartActionsScreen, true, true);
		} else if (_activeButton == (_buttonIndexOffset + 1)) {
			if (util::dsCRC8(_buffer, 7) == _buffer[7])
				ctx.show(APP->_confirmScreen, false, true);
			else
				ctx.show(APP->_errorScreen, false, true);
		}
	}
}
