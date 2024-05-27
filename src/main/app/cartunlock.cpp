
#include <stdio.h>
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/app/cartunlock.hpp"
#include "main/cartdata.hpp"
#include "main/uibase.hpp"

/* Pre-unlock cartridge screens */

static const util::Hash _UNLOCK_WARNINGS[cart::NUM_CHIP_TYPES]{
	0,
	"CartInfoScreen.unlockWarning.x76f041"_h,
	"CartInfoScreen.unlockWarning.x76f100"_h,
	"CartInfoScreen.unlockWarning.zs01"_h
};

enum IdentifyState {
	UNIDENTIFIED = 0,
	IDENTIFIED   = 1,
	UNKNOWN      = 2,
	BLANK_CART   = 2
};

static const util::Hash _LOCKED_PROMPTS[]{
	"CartInfoScreen.description.locked.unidentified"_h,
	"CartInfoScreen.description.locked.identified"_h,
	"CartInfoScreen.description.locked.unknown"_h
};
static const util::Hash _UNLOCKED_PROMPTS[]{
	"CartInfoScreen.description.unlocked.unidentified"_h,
	"CartInfoScreen.description.unlocked.identified"_h,
	"CartInfoScreen.description.unlocked.blank"_h
};

#define _PRINT(...) (ptr += snprintf(ptr, end - ptr __VA_OPT__(,) __VA_ARGS__))
#define _PRINTLN()  (*(ptr++) = '\n')

void CartInfoScreen::show(ui::Context &ctx, bool goBack) {
	_title = STR("CartInfoScreen.title");
	_body  = _bodyText;

	auto &dump = APP->_cartDump;

	char id1[32], id2[32], config[32];
	char *ptr = _bodyText, *end = &_bodyText[sizeof(_bodyText)];

	// Digital I/O board
	_PRINT(STR("CartInfoScreen.digitalIO.header"));

	if (dump.flags & cart::DUMP_SYSTEM_ID_OK) {
		dump.systemID.toString(id1);
		dump.systemID.toSerialNumber(id2);

		_PRINT(STR("CartInfoScreen.digitalIO.info"), id1, id2);
	} else if (dump.flags & cart::DUMP_HAS_SYSTEM_ID) {
		_PRINT(STR("CartInfoScreen.digitalIO.error"));
	} else {
		_PRINT(STR("CartInfoScreen.digitalIO.noBoard"));
	}

	_PRINTLN();

	// Security cartridge
	auto unlockStatus = (dump.flags & cart::DUMP_PRIVATE_DATA_OK)
		? STR("CartInfoScreen.unlockStatus.unlocked")
		: STR("CartInfoScreen.unlockStatus.locked");

	if (dump.flags & cart::DUMP_CART_ID_OK)
		dump.cartID.toString(id1);
	else if (dump.flags & cart::DUMP_HAS_CART_ID)
		__builtin_strcpy(id1, STR("CartInfoScreen.id.error"));
	else
		__builtin_strcpy(id1, STR("CartInfoScreen.id.noCartID"));

	if (dump.flags & cart::DUMP_CONFIG_OK)
		util::hexToString(config, dump.config, sizeof(dump.config), '-');
	else if (dump.flags & cart::DUMP_PRIVATE_DATA_OK)
		__builtin_strcpy(config, STR("CartInfoScreen.id.error"));
	else
		__builtin_strcpy(config, STR("CartInfoScreen.id.locked"));

	switch (dump.chipType) {
		case cart::NONE:
			_PRINT(STR("CartInfoScreen.description.noCart"));

			_prompt = STR("CartInfoScreen.prompt.error");
			goto _done;

		case cart::X76F041:
			_PRINT(STR("CartInfoScreen.cart.header"));
			_PRINT(
				STR("CartInfoScreen.cart.x76f041Info"), unlockStatus, id1,
				config
			);
			break;

		case cart::X76F100:
			_PRINT(STR("CartInfoScreen.cart.header"));
			_PRINT(STR("CartInfoScreen.cart.x76f100Info"), unlockStatus, id1);
			break;

		case cart::ZS01:
			if (!(dump.flags & cart::DUMP_PUBLIC_DATA_OK)) {
				_PRINT(STR("CartInfoScreen.description.initError"));

				_prompt = STR("CartInfoScreen.prompt.error");
				goto _done;
			}

			if (dump.flags & cart::DUMP_ZS_ID_OK)
				dump.zsID.toString(id2);
			else
				__builtin_strcpy(id2, STR("CartInfoScreen.id.error"));

			_PRINT(STR("CartInfoScreen.cart.header"));
			_PRINT(
				STR("CartInfoScreen.cart.zs01Info"), unlockStatus, id1, id2,
				config
			);
			break;
	}

	_PRINTLN();

	// At this point the cartridge can be in one of 8 states:
	// - locked, identified
	//   => unlock required, auto unlock available
	// - locked, parsed but unidentified
	//   => unlock required
	// - locked, parsing failed
	//   => unlock required
	// - locked, blank or no public data
	//   => unlock required
	// - unlocked, identified
	//   => all actions available
	// - unlocked, no private data, parsed but unidentified
	//   => all actions available (not implemented yet)
	// - unlocked, no private data, parsing failed
	//   => only dumping/flashing available
	// - unlocked, no private data, blank
	//   => only dumping/flashing available
	IdentifyState state;
	char          name[96], pairStatus[64];

	if (APP->_identified) {
		state = IDENTIFIED;
		APP->_identified->getDisplayName(name, sizeof(name));

		auto ids = APP->_cartParser->getIdentifiers();

		if (!(APP->_identified->flags & cart::DATA_HAS_SYSTEM_ID)) {
			__builtin_strcpy(
				pairStatus, STR("CartInfoScreen.pairing.unsupported")
			);
		} else if (!ids || !(dump.flags & cart::DUMP_PRIVATE_DATA_OK)) {
			__builtin_strcpy(pairStatus, STR("CartInfoScreen.pairing.unknown"));
		} else {
			auto &id = ids->systemID;

			id.toString(id1);
			id.toSerialNumber(id2);

			if (!__builtin_memcmp(id.data, dump.systemID.data, sizeof(id.data)))
				__builtin_strcpy(
					pairStatus, STR("CartInfoScreen.pairing.thisSystem")
				);
			else if (id.isEmpty())
				__builtin_strcpy(
					pairStatus, STR("CartInfoScreen.pairing.unpaired")
				);
			else
				snprintf(
					pairStatus, sizeof(pairStatus),
					STR("CartInfoScreen.pairing.otherSystem"), id1, id2
				);
		}
	} else if (
		dump.flags & (cart::DUMP_PUBLIC_DATA_OK | cart::DUMP_PRIVATE_DATA_OK)
	) {
		state = dump.isReadableDataEmpty() ? BLANK_CART : UNIDENTIFIED;
	} else {
		state = UNKNOWN;
	}

	// Description
	if (dump.flags & cart::DUMP_PRIVATE_DATA_OK) {
		_PRINT(STRH(_UNLOCKED_PROMPTS[state]), name, pairStatus);

		_prompt = STR("CartInfoScreen.prompt.unlocked");
	} else {
		_PRINT(STRH(_LOCKED_PROMPTS[state]), name, pairStatus);

		_prompt = STR("CartInfoScreen.prompt.locked");
	}

_done:
	//*(--ptr) = 0;
	LOG("remaining=%d", end - ptr);

	TextScreen::show(ctx, goBack);
}

void CartInfoScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT)) {
			ctx.show(APP->_mainMenuScreen, true, true);
		} else if (APP->_cartDump.chipType) {
			if (APP->_cartDump.flags & cart::DUMP_PRIVATE_DATA_OK)
				ctx.show(APP->_cartActionsScreen, false, true);
			else
				ctx.show(APP->_unlockKeyScreen, false, true);
		}
	}
}

struct SpecialEntry {
public:
	util::Hash name;
	void (UnlockKeyScreen::*target)(ui::Context &ctx);
};

static const SpecialEntry _SPECIAL_ENTRIES[]{
	{
		.name   = "UnlockKeyScreen.useFFKey"_h,
		.target = &UnlockKeyScreen::useFFKey
	}, {
		.name   = "UnlockKeyScreen.use00Key"_h,
		.target = &UnlockKeyScreen::use00Key
	}, {
		.name   = "UnlockKeyScreen.useCustomKey"_h,
		.target = &UnlockKeyScreen::useCustomKey
	}, {
		.name   = "UnlockKeyScreen.autoUnlock"_h,
		.target = &UnlockKeyScreen::autoUnlock
	}
};

int UnlockKeyScreen::_getNumSpecialEntries(ui::Context &ctx) const {
	int count = util::countOf(_SPECIAL_ENTRIES);

	if (!(APP->_identified))
		count--;

	return count;
}

const char *UnlockKeyScreen::_getItemName(ui::Context &ctx, int index) const {
	int offset = _getNumSpecialEntries(ctx);

	if (index < offset) {
		offset -= index + 1;
		return STRH(_SPECIAL_ENTRIES[offset].name);
	}

	static char name[96]; // TODO: get rid of this ugly crap

	APP->_cartDB.get(index - offset)->getDisplayName(name, sizeof(name));
	return name;
}

void UnlockKeyScreen::autoUnlock(ui::Context &ctx) {
	APP->_cartDump.copyKeyFrom(APP->_identified->dataKey);

	//APP->_selectedEntry = APP->_identified;
	APP->_selectedEntry = nullptr;
	ctx.show(APP->_confirmScreen, false, true);
}

void UnlockKeyScreen::useCustomKey(ui::Context &ctx) {
	APP->_selectedEntry = nullptr;
	ctx.show(APP->_keyEntryScreen, false, true);
}

void UnlockKeyScreen::use00Key(ui::Context &ctx) {
	util::clear(APP->_cartDump.dataKey, 0x00);

	APP->_selectedEntry = nullptr;
	ctx.show(APP->_confirmScreen, false, true);
}

void UnlockKeyScreen::useFFKey(ui::Context &ctx) {
	util::clear(APP->_cartDump.dataKey, 0xff);

	APP->_selectedEntry = nullptr;
	ctx.show(APP->_confirmScreen, false, true);
}

void UnlockKeyScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("UnlockKeyScreen.title");
	_prompt     = STR("UnlockKeyScreen.prompt");
	_itemPrompt = STR("UnlockKeyScreen.itemPrompt");

	_listLength = APP->_cartDB.getNumEntries() + _getNumSpecialEntries(ctx);

	ListScreen::show(ctx, goBack);
}

void UnlockKeyScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT)) {
			ctx.show(APP->_cartInfoScreen, true, true);
		} else {
			auto &dump  = APP->_cartDump;
			int  offset = _getNumSpecialEntries(ctx);

			APP->_confirmScreen.setMessage(
				APP->_unlockKeyScreen,
				[](ui::Context &ctx) {
					APP->_setupWorker(&App::_cartUnlockWorker);
					ctx.show(APP->_workerStatusScreen, false, true);
				},
				STRH(_UNLOCK_WARNINGS[dump.chipType])
			);

			if (_activeItem < offset) {
				offset -= _activeItem + 1;
				(this->*_SPECIAL_ENTRIES[offset].target)(ctx);
			} else {
				APP->_selectedEntry = APP->_cartDB.get(_activeItem - offset);

				dump.copyKeyFrom(APP->_selectedEntry->dataKey);
				ctx.show(APP->_confirmScreen, false, true);
			}
		}
	}
}

void KeyEntryScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("KeyEntryScreen.title");
	_body       = STR("KeyEntryScreen.body");
	_buttons[0] = STR("KeyEntryScreen.cancel");
	_buttons[1] = STR("KeyEntryScreen.ok");

	_numButtons   = 2;
	_bufferLength = 8;
	_separator    = '-';

	HexEntryScreen::show(ctx, goBack);
}

void KeyEntryScreen::update(ui::Context &ctx) {
	HexEntryScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (_activeButton == _buttonIndexOffset) {
			ctx.show(APP->_unlockKeyScreen, true, true);
		} else if (_activeButton == (_buttonIndexOffset + 1)) {
			auto &dump = APP->_cartDump;

			// TODO: deduplicate this code (it is the same as UnlockKeyScreen)
			APP->_confirmScreen.setMessage(
				APP->_unlockKeyScreen,
				[](ui::Context &ctx) {
					APP->_setupWorker(&App::_cartUnlockWorker);
					ctx.show(APP->_workerStatusScreen, false, true);
				},
				STRH(_UNLOCK_WARNINGS[dump.chipType])
			);

			dump.copyKeyFrom(_buffer);
			ctx.show(APP->_confirmScreen, false, true);
		}
	}
}
