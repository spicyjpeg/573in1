
#pragma once

#include "uibase.hpp"
#include "uicommon.hpp"
#include "util.hpp"

/* Main menu screens */

class WarningScreen : public ui::MessageBoxScreen {
private:
	int  _cooldownTimer;
	char _buttonText[16];

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class ButtonMappingScreen : public ui::ListScreen {
protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class MainMenuScreen : public ui::ListScreen {
protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void cartInfo(ui::Context &ctx);
	void dump(ui::Context &ctx);
	void restore(ui::Context &ctx);
	void systemInfo(ui::Context &ctx);
	void setResolution(ui::Context &ctx);
	void about(ui::Context &ctx);
	void ejectCD(ui::Context &ctx);
	void reboot(ui::Context &ctx);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

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
