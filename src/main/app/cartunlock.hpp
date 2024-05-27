
#pragma once

#include "main/uibase.hpp"
#include "main/uicommon.hpp"
#include "main/uimodals.hpp"

/* Pre-unlock cartridge screens */

class CartInfoScreen : public ui::TextScreen {
private:
	char _bodyText[2048];

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class UnlockKeyScreen : public ui::ListScreen {
private:
	int _getNumSpecialEntries(ui::Context &ctx) const;

protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void autoUnlock(ui::Context &ctx);
	void useCustomKey(ui::Context &ctx);
	void use00Key(ui::Context &ctx);
	void useFFKey(ui::Context &ctx);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class KeyEntryScreen : public ui::HexEntryScreen {
public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};
