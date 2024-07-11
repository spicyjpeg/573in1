/*
 * 573in1 - Copyright (C) 2022-2024 spicyjpeg
 *
 * 573in1 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * 573in1 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * 573in1. If not, see <https://www.gnu.org/licenses/>.
 */

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
