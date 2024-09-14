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

#include "common/fs/file.hpp"
#include "common/storage/device.hpp"
#include "common/util/templates.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"
#include "main/uimodals.hpp"

/* Modal screens */

class WorkerStatusScreen : public ui::ProgressScreen {
public:
	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

static constexpr size_t NUM_MESSAGE_TYPES = 3;

enum MessageType {
	MESSAGE_SUCCESS = 0,
	MESSAGE_WARNING = 1,
	MESSAGE_ERROR   = 2
};

class MessageScreen : public ui::MessageBoxScreen {
private:
	MessageType _type;
	char        _bodyText[512];

public:
	ui::Screen *previousScreens[NUM_MESSAGE_TYPES];

	void setMessage(MessageType type, const char *format, ...);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class ConfirmScreen : public ui::MessageBoxScreen {
private:
	char _bodyText[512];
	void (*_callback)(ui::Context &ctx);

public:
	ui::Screen *previousScreen;

	void setMessage(
		void (*callback)(ui::Context &ctx), const char *format, ...
	);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

/* File picker screen */

static constexpr size_t MAX_FILE_PICKER_DEVICES = 4;

struct FilePickerEntry {
public:
	storage::Device *dev;
	fs::Provider    *provider;
	const char      *prefix;
};

class FilePickerScreen : public ui::ListScreen {
	friend class FileBrowserScreen;

private:
	char _promptText[512];
	void (*_callback)(ui::Context &ctx);

	FilePickerEntry _entries[MAX_FILE_PICKER_DEVICES];

	void _addDevice(
		storage::Device *dev, fs::Provider *provider, const char *prefix
	);

protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	ui::Screen *previousScreen;

	void setMessage(
		void (*callback)(ui::Context &ctx), const char *format, ...
	);
	void reloadAndShow(ui::Context &ctx);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};

class FileBrowserScreen : public ui::ListScreen {
private:
	char _currentPath[fs::MAX_PATH_LENGTH];
	bool _isRoot;

	int        _numFiles, _numDirectories;
	util::Data _files, _directories;

	void _setPathToParent(void);
	void _setPathToChild(const char *entry);
	void _unloadDirectory(void);

protected:
	const char *_getItemName(ui::Context &ctx, int index) const;

public:
	char selectedPath[fs::MAX_PATH_LENGTH];

	int loadDirectory(
		ui::Context &ctx, const char *path, bool updateCurrent = true
	);

	void show(ui::Context &ctx, bool goBack = false);
	void update(ui::Context &ctx);
};
