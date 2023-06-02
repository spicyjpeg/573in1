
#include <stdio.h>
#include <string.h>
#include "app/app.hpp"
#include "app/unlock.hpp"
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
		.warning = "CartInfoScreen.noCart.warning"_h,
		.error   = 0
	}, {
		.name    = "CartInfoScreen.x76f041.name"_h,
		.warning = "CartInfoScreen.x76f041.warning"_h,
		.error   = 0
	}, {
		.name    = "CartInfoScreen.x76f100.name"_h,
		.warning = "CartInfoScreen.x76f100.warning"_h,
		.error   = 0
	}, {
		.name    = "CartInfoScreen.zs01.name"_h,
		.warning = "CartInfoScreen.zs01.warning"_h,
		.error   = 0
	}
};

enum IdentifyState {
	UNIDENTIFIED = 0,
	IDENTIFIED   = 1,
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

	char id1[32], id2[32];
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

	ptr += snprintf(ptr, end - ptr, STR("CartInfoScreen.digitalIOInfo"), id1, id2);

	// Cartridge info
	if (!dump.chipType) {
		memccpy(ptr, STR("CartInfoScreen.description.noCart"), 0, end - ptr);

		_prompt = STR("CartInfoScreen.prompt.error");
		return;
	} else if (!(dump.flags & cart::DUMP_PUBLIC_DATA_OK)) {
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

	auto unlockStatus = (dump.flags & cart::DUMP_PRIVATE_DATA_OK) ?
		STR("CartInfoScreen.unlockStatus.unlocked") :
		STR("CartInfoScreen.unlockStatus.locked");

	ptr += snprintf(
		ptr, end - ptr, STR("CartInfoScreen.cartInfo"),
		STRH(_CART_TYPES[dump.chipType].name), unlockStatus, id1, id2
	);

	// At this point the cartridge can be in one of 6 states:
	// - locked, identified
	//   => unlock required, auto unlock available
	// - locked, unidentified
	//   => unlock required
	// - locked, blank or no public data
	//   => unlock required
	// - unlocked, identified
	//   => all actions available
	// - unlocked, no private data, unidentified
	//   => only dumping/flashing available
	// - unlocked, no private data, blank
	//   => only dumping/flashing available
	IdentifyState state;
	char          name[96];

	if (APP->_identified) {
		state = IDENTIFIED;
		APP->_identified->getDisplayName(name, sizeof(name));
	} else {
		state = APP->_dump.isDataEmpty() ? BLANK_CART : UNIDENTIFIED;
	}

	if (dump.flags & cart::DUMP_PRIVATE_DATA_OK) {
		ptr += snprintf(ptr, end - ptr, STRH(_UNLOCKED_PROMPTS[state]), name);

		_prompt = STR("CartInfoScreen.prompt.unlocked");
	} else {
		ptr += snprintf(ptr, end - ptr, STRH(_LOCKED_PROMPTS[state]), name);

		_prompt = STR("CartInfoScreen.prompt.locked");
	}
}

void CartInfoScreen::update(ui::Context &ctx) {
	auto &dump = APP->_dump;

	if (!dump.chipType)
		return;

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (dump.flags & cart::DUMP_PRIVATE_DATA_OK)
			ctx.show(APP->_cartActionsScreen, false, true);
		else
			ctx.show(APP->_unlockKeyScreen, false, true);
	}
}

enum SpecialUnlockKeyEntry {
	ENTRY_AUTO_UNLOCK = -3,
	ENTRY_CUSTOM_KEY  = -2,
	ENTRY_NULL_KEY    = -1
};

int UnlockKeyScreen::_getSpecialEntryOffset(ui::Context &ctx) const {
	return APP->_identified ? ENTRY_AUTO_UNLOCK : ENTRY_CUSTOM_KEY;
}

const char *UnlockKeyScreen::_getItemName(ui::Context &ctx, int index) const {
	static char name[96]; // TODO: get rid of this ugly crap

	index += _getSpecialEntryOffset(ctx);

	switch (index) {
		case ENTRY_AUTO_UNLOCK:
			return _autoUnlockItem;

		case ENTRY_CUSTOM_KEY:
			return _customKeyItem;

		case ENTRY_NULL_KEY:
			return _nullKeyItem;

		default:
			APP->_db.get(index)->getDisplayName(name, sizeof(name));
			return name;
	}
}

void UnlockKeyScreen::show(ui::Context &ctx, bool goBack) {
	_title          = STR("UnlockKeyScreen.title");
	_prompt         = STR("UnlockKeyScreen.prompt");
	_itemPrompt     = STR("UnlockKeyScreen.itemPrompt");
	_autoUnlockItem = STR("UnlockKeyScreen.autoUnlock");
	_customKeyItem  = STR("UnlockKeyScreen.customKey");
	_nullKeyItem    = STR("UnlockKeyScreen.nullKey");

	_listLength = APP->_db.getNumEntries() - _getSpecialEntryOffset(ctx);

	ListScreen::show(ctx, goBack);
}

void UnlockKeyScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		int index = _activeItem + _getSpecialEntryOffset(ctx);

		switch (index) {
			case ENTRY_AUTO_UNLOCK:
				__builtin_memcpy(
					APP->_dump.dataKey, APP->_identified->dataKey,
					sizeof(APP->_dump.dataKey)
				);
				ctx.show(APP->_unlockConfirmScreen, false, true);
				break;

			case ENTRY_CUSTOM_KEY:
				//ctx.show(APP->_unlockKeyEntryScreen, false, true);
				break;

			case ENTRY_NULL_KEY:
				__builtin_memset(
					APP->_dump.dataKey, 0, sizeof(APP->_dump.dataKey)
				);
				ctx.show(APP->_unlockConfirmScreen, false, true);
				break;

			default:
				__builtin_memcpy(
					APP->_dump.dataKey, APP->_db.get(index)->dataKey,
					sizeof(APP->_dump.dataKey)
				);
				ctx.show(APP->_unlockConfirmScreen, false, true);
		}
	} else if (ctx.buttons.held(ui::BTN_LEFT) && ctx.buttons.held(ui::BTN_RIGHT)) {
		ctx.show(APP->_cartInfoScreen, true, true);
	}
}

void UnlockConfirmScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("UnlockConfirmScreen.title");
	_body       = STRH(_CART_TYPES[APP->_dump.chipType].warning);
	_buttons[0] = STR("UnlockConfirmScreen.no");
	_buttons[1] = STR("UnlockConfirmScreen.yes");

	_numButtons = 2;

	MessageScreen::show(ctx, goBack);
}

void UnlockConfirmScreen::update(ui::Context &ctx) {
	MessageScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (_activeButton) {
			APP->_setupWorker(&App::_cartUnlockWorker);
			ctx.show(APP->_workerStatusScreen, false, true);
		} else {
			ctx.show(APP->_unlockKeyScreen, true, true);
		}
	}
}

void UnlockErrorScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("UnlockErrorScreen.title");
	_body       = STRH(_CART_TYPES[APP->_dump.chipType].error);
	_buttons[0] = STR("UnlockErrorScreen.ok");

	_numButtons = 1;

	MessageScreen::show(ctx, goBack);
}

void UnlockErrorScreen::update(ui::Context &ctx) {
	MessageScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_cartInfoScreen, true, true);
}
