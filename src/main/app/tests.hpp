
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
