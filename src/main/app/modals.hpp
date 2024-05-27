
#pragma once

#include "common/file.hpp"
#include "common/ide.hpp"
#include "common/util.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"
#include "main/uimodals.hpp"

/* Modal screens */

class WorkerStatusScreen : public ui::ProgressScreen {
public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

enum MessageType {
	MESSAGE_SUCCESS = 0,
	MESSAGE_WARNING = 1,
	MESSAGE_ERROR   = 2
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

/* File picker screen */

class FilePickerScreen : public ui::ListScreen {
	friend class FileBrowserScreen;

private:
	char       _promptText[512];
	ui::Screen *_prevScreen;
	void       (*_callback)(ui::Context &ctx);

	int _drives[util::countOf(ide::devices)];

protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	void setMessage(
		ui::Screen &prev, void (*callback)(ui::Context &ctx),
		const char *format, ...
	);
	int loadRootAndShow(ui::Context &ctx);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class FileBrowserScreen : public ui::ListScreen {
private:
	char _currentPath[file::MAX_PATH_LENGTH];
	bool _isRoot;

	int        _numFiles, _numDirectories;
	util::Data _files, _directories;

	void _setPathToParent(void);
	void _setPathToChild(const char *entry);
	void _unloadDirectory(void);

protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	char selectedPath[file::MAX_PATH_LENGTH];

	int loadDirectory(
		ui::Context &ctx, const char *path, bool updateCurrent = true
	);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};
