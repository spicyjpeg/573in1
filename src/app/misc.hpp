
#pragma once

#include "uibase.hpp"
#include "uicommon.hpp"

/* Initial setup screens */

class WorkerStatusScreen : public ui::ProgressScreen {
public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class WarningScreen : public ui::MessageScreen {
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

class ErrorScreen : public ui::MessageScreen {
private:
	char       _bodyText[512];
	ui::Screen *_prevScreen;

public:
	void setMessage(ui::Screen &prev, const char *format, ...);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class ConfirmScreen : public ui::MessageScreen {
private:
	char       _bodyText[512];
	ui::Screen *_prevScreen;
	void       (*_callback)(ui::Context &ctx);

public:
	void setMessage(
		ui::Screen &prev, void (*callback)(ui::Context &ctx),
		const char *format, ...
	);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};
