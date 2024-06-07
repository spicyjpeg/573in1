
#pragma once

#include "main/uibase.hpp"
#include "main/uicommon.hpp"
#include "main/uimodals.hpp"

/* Main menu screens */

class WarningScreen : public ui::MessageBoxScreen {
private:
	int  _timer;
	char _buttonText[16];

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class AutobootScreen : public ui::MessageBoxScreen {
private:
	int  _timer;
	char _bodyText[512], _buttonText[16];

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
	void storageInfo(ui::Context &ctx);
	void ideInfo(ui::Context &ctx);
	void runExecutable(ui::Context &ctx);
	void setRTCTime(ui::Context &ctx);
	void testMenu(ui::Context &ctx);
	void setResolution(ui::Context &ctx);
	void about(ui::Context &ctx);
	void ejectCD(ui::Context &ctx);
	void reboot(ui::Context &ctx);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};
