
#include <stdio.h>
#include <string.h>
#include "app/app.hpp"
#include "app/cartunlock.hpp"
#include "cartdata.hpp"
#include "cartio.hpp"
#include "defs.hpp"
#include "uibase.hpp"
#include "util.hpp"

/* Pre-unlock cartridge screens */

struct CartType {
public:
	util::Hash name, warning, error;
};

static const CartType _CART_TYPES[cart::NUM_CHIP_TYPES]{
	{
		.name    = "CartInfoScreen.noCart.name"_h,
		.warning = 0,
		.error   = 0
	}, {
		.name    = "CartInfoScreen.x76f041.name"_h,
		.warning = "CartInfoScreen.x76f041.warning"_h,
		.error   = "CartInfoScreen.x76f041.error"_h
	}, {
		.name    = "CartInfoScreen.x76f100.name"_h,
		.warning = "CartInfoScreen.x76f100.warning"_h,
		.error   = "CartInfoScreen.x76f100.error"_h
	}, {
		.name    = "CartInfoScreen.zs01.name"_h,
		.warning = "CartInfoScreen.zs01.warning"_h,
		.error   = "CartInfoScreen.zs01.error"_h
	}
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

void CartInfoScreen::show(ui::Context &ctx, bool goBack) {
	_title = STR("CartInfoScreen.title");
	_body  = _bodyText;

	TextScreen::show(ctx, goBack);

	auto &dump = APP->_dump;

	char id1[32], id2[32], config[32];
	char *ptr = _bodyText, *end = &_bodyText[sizeof(_bodyText)];

	// Digital I/O board info
	if (dump.flags & cart::DUMP_SYSTEM_ID_OK) {
		dump.systemID.toString(id1);
		dump.systemID.toSerialNumber(id2);
	} else if (dump.flags & cart::DUMP_HAS_SYSTEM_ID) {
		__builtin_strcpy(id1, STR("CartInfoScreen.id.error"));
		__builtin_strcpy(id2, id1);
	} else {
		__builtin_strcpy(id1, STR("CartInfoScreen.id.noSystemID"));
		__builtin_strcpy(id2, id1);
	}

	ptr += snprintf(
		ptr, end - ptr, STR("CartInfoScreen.digitalIOInfo"), id1, id2
	);

	// Cartridge info
	if (!dump.chipType) {
		memccpy(ptr, STR("CartInfoScreen.description.noCart"), 0, end - ptr);

		_prompt = STR("CartInfoScreen.prompt.error");
		return;
	}
	if (
		//dump.getChipSize().publicDataLength &&
		(dump.chipType == cart::ZS01) &&
		!(dump.flags & cart::DUMP_PUBLIC_DATA_OK)
	) {
		memccpy(ptr, STR("CartInfoScreen.description.initError"), 0, end - ptr);

		_prompt = STR("CartInfoScreen.prompt.error");
		return;
	}

	if (dump.flags & cart::DUMP_CART_ID_OK)
		dump.cartID.toString(id1);
	else if (dump.flags & cart::DUMP_HAS_CART_ID)
		__builtin_strcpy(id1, STR("CartInfoScreen.id.error"));
	else
		__builtin_strcpy(id1, STR("CartInfoScreen.id.noCartID"));

	if (dump.flags & cart::DUMP_ZS_ID_OK)
		dump.zsID.toString(id2);
	else if (dump.chipType == cart::ZS01)
		__builtin_strcpy(id2, STR("CartInfoScreen.id.error"));
	else
		__builtin_strcpy(id2, STR("CartInfoScreen.id.noZSID"));

	if (dump.flags & cart::DUMP_CONFIG_OK)
		util::hexToString(config, dump.config, sizeof(dump.config), '-');
	else if (dump.flags & cart::DUMP_PRIVATE_DATA_OK)
		__builtin_strcpy(config, STR("CartInfoScreen.config.error"));
	else
		__builtin_strcpy(config, STR("CartInfoScreen.config.locked"));

	auto unlockStatus = (dump.flags & cart::DUMP_PRIVATE_DATA_OK)
		? STR("CartInfoScreen.unlockStatus.unlocked")
		: STR("CartInfoScreen.unlockStatus.locked");

	ptr += snprintf(
		ptr, end - ptr, STR("CartInfoScreen.cartInfo"),
		STRH(_CART_TYPES[dump.chipType].name), unlockStatus, id1, id2, config
	);

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

		auto ids = APP->_parser->getIdentifiers();

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
		state = APP->_dump.isReadableDataEmpty() ? BLANK_CART : UNIDENTIFIED;
	} else {
		state = UNKNOWN;
	}

	if (dump.flags & cart::DUMP_PRIVATE_DATA_OK) {
		ptr += snprintf(
			ptr, end - ptr, STRH(_UNLOCKED_PROMPTS[state]), name, pairStatus
		);

		_prompt = STR("CartInfoScreen.prompt.unlocked");
	} else {
		ptr += snprintf(
			ptr, end - ptr, STRH(_LOCKED_PROMPTS[state]), name, pairStatus
		);

		_prompt = STR("CartInfoScreen.prompt.locked");
	}
}

void CartInfoScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (APP->_dump.chipType && ctx.buttons.pressed(ui::BTN_START)) {
		if (APP->_dump.flags & cart::DUMP_PRIVATE_DATA_OK)
			ctx.show(APP->_cartActionsScreen, false, true);
		else
			ctx.show(APP->_unlockKeyScreen, false, true);
	}
	if (
		(ctx.buttons.held(ui::BTN_LEFT) && ctx.buttons.pressed(ui::BTN_RIGHT)) ||
		(ctx.buttons.pressed(ui::BTN_LEFT) && ctx.buttons.held(ui::BTN_RIGHT))
	)
		ctx.show(APP->_mainMenuScreen, true, true);
}

enum SpecialEntryIndex {
	ENTRY_AUTO_UNLOCK = -4,
	ENTRY_CUSTOM_KEY  = -3,
	ENTRY_NULL_KEY1   = -2,
	ENTRY_NULL_KEY2   = -1
};

struct SpecialEntry {
public:
	util::Hash name;
	void (UnlockKeyScreen::*target)(ui::Context &ctx);
};

static const SpecialEntry _SPECIAL_ENTRIES[]{
	{
		.name   = 0,
		.target = nullptr
	}, {
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

int UnlockKeyScreen::_getSpecialEntryOffset(ui::Context &ctx) const {
	return APP->_identified ? ENTRY_AUTO_UNLOCK : ENTRY_CUSTOM_KEY;
}

const char *UnlockKeyScreen::_getItemName(ui::Context &ctx, int index) const {
	index += _getSpecialEntryOffset(ctx);

	if (index < 0)
		return STRH(_SPECIAL_ENTRIES[-index].name);

	static char name[96]; // TODO: get rid of this ugly crap

	APP->_db.get(index)->getDisplayName(name, sizeof(name));
	return name;
}

void UnlockKeyScreen::autoUnlock(ui::Context &ctx) {
	__builtin_memcpy(
		APP->_dump.dataKey, APP->_identified->dataKey,
		sizeof(APP->_dump.dataKey)
	);

	//APP->_selectedEntry = APP->_identified;
	APP->_selectedEntry = nullptr;
	ctx.show(APP->_confirmScreen, false, true);
}

void UnlockKeyScreen::useCustomKey(ui::Context &ctx) {
	APP->_selectedEntry = nullptr;
	ctx.show(APP->_keyEntryScreen, false, true);
}

void UnlockKeyScreen::use00Key(ui::Context &ctx) {
	__builtin_memset(
		APP->_dump.dataKey, 0x00, sizeof(APP->_dump.dataKey)
	);

	APP->_selectedEntry = nullptr;
	ctx.show(APP->_confirmScreen, false, true);
}

void UnlockKeyScreen::useFFKey(ui::Context &ctx) {
	__builtin_memset(
		APP->_dump.dataKey, 0xff, sizeof(APP->_dump.dataKey)
	);

	APP->_selectedEntry = nullptr;
	ctx.show(APP->_confirmScreen, false, true);
}

void UnlockKeyScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("UnlockKeyScreen.title");
	_prompt     = STR("UnlockKeyScreen.prompt");
	_itemPrompt = STR("UnlockKeyScreen.itemPrompt");

	_listLength = APP->_db.getNumEntries() - _getSpecialEntryOffset(ctx);

	ListScreen::show(ctx, goBack);
}

void UnlockKeyScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		int index = _activeItem + _getSpecialEntryOffset(ctx);

		APP->_confirmScreen.setMessage(
			APP->_unlockKeyScreen,
			[](ui::Context &ctx) {
				APP->_setupWorker(&App::_cartUnlockWorker);
				ctx.show(APP->_workerStatusScreen, false, true);
			},
			STRH(_CART_TYPES[APP->_dump.chipType].warning)
		);

		APP->_messageScreen.setMessage(
			MESSAGE_ERROR, APP->_cartInfoScreen,
			STRH(_CART_TYPES[APP->_dump.chipType].error)
		);

		if (index < 0) {
			(this->*_SPECIAL_ENTRIES[-index].target)(ctx);
		} else {
			__builtin_memcpy(
				APP->_dump.dataKey, APP->_db.get(index)->dataKey,
				sizeof(APP->_dump.dataKey)
			);

			APP->_selectedEntry = APP->_db.get(index);
			ctx.show(APP->_confirmScreen, false, true);
		}
	}
	if (
		(ctx.buttons.held(ui::BTN_LEFT) && ctx.buttons.pressed(ui::BTN_RIGHT)) ||
		(ctx.buttons.pressed(ui::BTN_LEFT) && ctx.buttons.held(ui::BTN_RIGHT))
	) {
		ctx.show(APP->_cartInfoScreen, true, true);
	}
}

void KeyEntryScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("KeyEntryScreen.title");
	_body       = STR("KeyEntryScreen.body");
	_buttons[0] = STR("KeyEntryScreen.cancel");
	_buttons[1] = STR("KeyEntryScreen.ok");

	_numButtons = 2;
	_locked     = false;

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
			// TODO: deduplicate this code (it is the same as UnlockKeyScreen)
			APP->_confirmScreen.setMessage(
				APP->_unlockKeyScreen,
				[](ui::Context &ctx) {
					APP->_setupWorker(&App::_cartUnlockWorker);
					ctx.show(APP->_workerStatusScreen, false, true);
				},
				STRH(_CART_TYPES[APP->_dump.chipType].warning)
			);

			APP->_messageScreen.setMessage(
				MESSAGE_ERROR, APP->_cartInfoScreen,
				STRH(_CART_TYPES[APP->_dump.chipType].error)
			);

			__builtin_memcpy(
				APP->_dump.dataKey, _buffer, sizeof(APP->_dump.dataKey)
			);
			ctx.show(APP->_confirmScreen, false, true);
		}
	}
}
