
#pragma once

#include "main/uibase.hpp"
#include "main/uicommon.hpp"

/* Modal screens */

class WorkerStatusScreen : public ui::ProgressScreen {
public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

enum MessageType {
	MESSAGE_SUCCESS = 0,
	MESSAGE_ERROR   = 1
};

class MessageScreen : public ui::MessageBoxScreen {
private:
	MessageType _type;
	char        _bodyText[512];
	ui::Screen  *_prevScreen;

public:
	void setMessage(
		MessageType type, ui::Screen &prev, const char *format, ...
	);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class ConfirmScreen : public ui::MessageBoxScreen {
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
