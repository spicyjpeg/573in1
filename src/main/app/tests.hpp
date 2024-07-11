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

/* Top-level test menu */

class TestMenuScreen : public ui::ListScreen {
protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void jammaTest(ui::Context &ctx);
	void audioTest(ui::Context &ctx);
	void colorIntensity(ui::Context &ctx);
	void geometry(ui::Context &ctx);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

/* Test submenus */

class JAMMATestScreen : public ui::TextScreen {
private:
	char _bodyText[2048];

public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class AudioTestScreen : public ui::ListScreen {
protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void playLeft(ui::Context &ctx);
	void playRight(ui::Context &ctx);
	void playBoth(ui::Context &ctx);
	void enableAmp(ui::Context &ctx);
	void disableAmp(ui::Context &ctx);
	void enableCDDA(ui::Context &ctx);
	void disableCDDA(ui::Context &ctx);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

/* Test pattern screens */

class TestPatternScreen : public ui::AnimatedScreen {
protected:
	void _drawTextOverlay(
		ui::Context &ctx, const char *title, const char *prompt
	) const;

public:
	virtual void draw(ui::Context &ctx, bool active) const;
	virtual void update(ui::Context &ctx);
};

class ColorIntensityScreen : public TestPatternScreen {
public:
	void draw(ui::Context &ctx, bool active) const;
};

class GeometryScreen : public TestPatternScreen {
public:
	void draw(ui::Context &ctx, bool active) const;
};
