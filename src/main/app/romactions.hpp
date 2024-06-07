
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/rom.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"
#include "main/uimodals.hpp"

/* Storage device submenu */

class StorageInfoScreen : public ui::TextScreen {
private:
	char _bodyText[2048];

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class StorageActionsScreen : public ui::ListScreen {
protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	const rom::Region *selectedRegion;
	size_t            selectedLength;

	void runExecutable(ui::Context &ctx, size_t length);
	void checksum(ui::Context &ctx, size_t length);
	void dump(ui::Context &ctx, size_t length);
	void restore(ui::Context &ctx, size_t length);
	void erase(ui::Context &ctx, size_t length);
	void resetFlashHeader(ui::Context &ctx, size_t length);
	void matchFlashHeader(ui::Context &ctx, size_t length);
	void editFlashHeader(ui::Context &ctx, size_t length);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class CardSizeScreen : public ui::MessageBoxScreen {
public:
	void (StorageActionsScreen::*callback)(ui::Context &ctx, size_t length);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

// The CRCs have to be wrapped into their own structure in order to allow usage
// of offsetof().
struct ChecksumValues {
public:
	uint32_t bios, rtc, flash;
	uint32_t pcmcia[2][4];
};

class ChecksumScreen : public ui::TextScreen {
private:
	char _bodyText[2048];

public:
	bool           valid;
	ChecksumValues values;

	inline ChecksumScreen(void)
	: valid(false) {}

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};
