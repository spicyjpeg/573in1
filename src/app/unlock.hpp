
#pragma once

#include "uibase.hpp"
#include "uicommon.hpp"

/* Pre-unlock cartridge screens */

class CartInfoScreen : public ui::TextScreen {
private:
	char _bodyText[1024];

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class UnlockKeyScreen : public ui::ListScreen {
private:
	const char *_autoUnlockItem, *_customKeyItem, *_nullKeyItem;

	int _getSpecialEntryOffset(ui::Context &ctx) const;

protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class UnlockConfirmScreen : public ui::MessageScreen {
public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class UnlockErrorScreen : public ui::MessageScreen {
public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};
