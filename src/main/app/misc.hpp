
#pragma once

#include "common/rom.hpp"
#include "common/util.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"

/* Storage device submenu */

class StorageMenuScreen : public ui::ListScreen {
private:
	const rom::Region *_selectedRegion;

protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	inline const rom::Region &getSelectedRegion(void) {
		return *_selectedRegion;
	}

	void dump(ui::Context &ctx);
	void restore(ui::Context &ctx);
	void erase(ui::Context &ctx);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

/* System information screen */

class SystemInfoScreen : public ui::TextScreen {
private:
	char _bodyText[2048];

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

/* Misc. screens */

class ResolutionScreen : public ui::ListScreen {
protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class AboutScreen : public ui::TextScreen {
private:
	util::Data _text;

public:
	void show(ui::Context &ctx, bool goBack = false);
	void hide(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};
