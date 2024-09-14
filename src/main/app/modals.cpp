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

#include <stdarg.h>
#include <stdio.h>
#include "common/fs/file.hpp"
#include "common/fs/misc.hpp"
#include "common/storage/device.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "common/defs.hpp"
#include "main/app/app.hpp"
#include "main/app/modals.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"

/* Modal screens */

void WorkerStatusScreen::show(ui::Context &ctx, bool goBack) {
	_title = STR("WorkerStatusScreen.title");

	ProgressScreen::show(ctx, goBack);
}

void WorkerStatusScreen::update(ui::Context &ctx) {
	auto &worker = APP->_workerStatus;

	if (worker.status == WORKER_DONE) {
		worker.setStatus(WORKER_IDLE);
		ctx.show(*worker.nextScreen, worker.nextGoBack);
		return;
	}

	_setProgress(ctx, worker.progress, worker.progressTotal);
	_body = worker.message;
}

static const util::Hash _MESSAGE_TITLES[]{
	"MessageScreen.title.success"_h,
	"MessageScreen.title.warning"_h,
	"MessageScreen.title.error"_h
};

void MessageScreen::setMessage(MessageType type, const char *format, ...) {
	_type = type;

	va_list ap;

	va_start(ap, format);
	vsnprintf(_bodyText, sizeof(_bodyText), format, ap);
	va_end(ap);
}

void MessageScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STRH(_MESSAGE_TITLES[_type]);
	_body       = _bodyText;
	_buttons[0] = STR("MessageScreen.ok");

	_numButtons = 1;
	_locked     = !previousScreens[_type];

	MessageBoxScreen::show(ctx, goBack);
	ctx.sounds[ui::SOUND_ALERT].play();
}

void MessageScreen::update(ui::Context &ctx) {
	MessageBoxScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(*previousScreens[_type], true, true);
}

void ConfirmScreen::setMessage(
	void (*callback)(ui::Context &ctx), const char *format, ...
) {
	_callback = callback;

	va_list ap;

	va_start(ap, format);
	vsnprintf(_bodyText, sizeof(_bodyText), format, ap);
	va_end(ap);
}

void ConfirmScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("ConfirmScreen.title");
	_body       = _bodyText;
	_buttons[0] = STR("ConfirmScreen.no");
	_buttons[1] = STR("ConfirmScreen.yes");

	_numButtons = 2;

	MessageBoxScreen::show(ctx, goBack);
#if 0
	ctx.sounds[ui::SOUND_ALERT].play();
#endif
}

void ConfirmScreen::update(ui::Context &ctx) {
	MessageBoxScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (_activeButton)
			_callback(ctx);
		else
			ctx.show(*previousScreen, true, true);
	}
}

/* File picker screen */

struct DeviceType {
public:
	const char *format;
	util::Hash error;
};

static const DeviceType _DEVICE_TYPES[]{
	{
		// storage::NONE
		.format = CH_HOST_ICON " %s",
		.error  = "FilePickerScreen.hostError"_h
	}, {
		// storage::ATA
		.format = CH_HDD_ICON " %s: %s",
		.error  = "FilePickerScreen.ataError"_h
	}, {
		// storage::ATAPI
		.format = CH_CDROM_ICON " %s: %s",
		.error  = "FilePickerScreen.atapiError"_h
	}
};

void FilePickerScreen::_addDevice(
	storage::Device *dev, fs::Provider *provider, const char *prefix
) {
	// Note that devices are added (and thus displayed in the list) even if
	// their filesystem is unrecognized and no file provider is available.
	if (!dev && !provider)
		return;
	if (_listLength >= int(MAX_FILE_PICKER_DEVICES))
		return;

	auto &entry = _entries[_listLength++];

	entry.dev      = dev;
	entry.provider = provider;
	entry.prefix   = prefix;
}

const char *FilePickerScreen::_getItemName(ui::Context &ctx, int index) const {
	static char name[fs::MAX_NAME_LENGTH]; // TODO: get rid of this ugly crap

	auto &entry = _entries[index];
	auto label  = entry.provider
		? entry.provider->volumeLabel
		: STR("FilePickerScreen.noFS");

	if (entry.dev)
		snprintf(
			name, sizeof(name), _DEVICE_TYPES[entry.dev->type].format,
			entry.dev->model, label
		);
	else
		snprintf(
			name, sizeof(name), _DEVICE_TYPES[storage::NONE].format, label
		);

	return name;
}

void FilePickerScreen::setMessage(
	void (*callback)(ui::Context &ctx), const char *format, ...
) {
	_callback = callback;

	va_list ap;

	va_start(ap, format);
	vsnprintf(_promptText, sizeof(_promptText), format, ap);
	va_end(ap);
}

void FilePickerScreen::reloadAndShow(ui::Context &ctx) {
	// Check if any drive has reported a disc change and reload all filesystems
	// if necessary.
	for (auto dev : APP->_fileIO.ideDevices) {
		if (!dev)
			continue;
		if (!dev->poll())
			continue;

		APP->_messageScreen.previousScreens[MESSAGE_ERROR] = this;

		APP->_runWorker(&App::_fileInitWorker, *this, false, true);
		return;
	}

	ctx.show(*this, false, true);
}

void FilePickerScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("FilePickerScreen.title");
	_prompt     = _promptText;
	_itemPrompt = STR("FilePickerScreen.itemPrompt");

	_listLength = 0;

#ifdef ENABLE_PCDRV
	_addDevice(nullptr, APP->_fileIO.host, "host:");
#endif

	for (size_t i = 0; i < util::countOf(APP->_fileIO.ideDevices); i++)
		_addDevice(
			APP->_fileIO.ideDevices[i], APP->_fileIO.ideProviders[i],
			IDE_MOUNT_POINTS[i]
		);

	ListScreen::show(ctx, goBack);
}

void FilePickerScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (!_listLength) {
		APP->_messageScreen.previousScreens[MESSAGE_ERROR] = previousScreen;
		APP->_messageScreen.setMessage(
			MESSAGE_ERROR, STR("FilePickerScreen.noDeviceError")
		);
		ctx.show(APP->_messageScreen, false, true);
		return;
	}

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT)) {
			ctx.show(*previousScreen, true, true);
		} else {
			auto &entry = _entries[_activeItem];
			auto type   = entry.dev ? entry.dev->type : storage::NONE;

			int count =
				APP->_fileBrowserScreen.loadDirectory(ctx, entry.prefix);

			if (count > 0) {
				ctx.show(APP->_fileBrowserScreen, false, true);
			} else {
				util::Hash error;

				if (!count)
					error = "FilePickerScreen.noFilesError"_h;
				else
					error = _DEVICE_TYPES[type].error;

				APP->_messageScreen.previousScreens[MESSAGE_ERROR] = this;
				APP->_messageScreen.setMessage(MESSAGE_ERROR, STRH(error));
				ctx.show(APP->_messageScreen, false, true);
			}
		}
	}
}

void FileBrowserScreen::_setPathToParent(void) {
	auto ptr = __builtin_strrchr(_currentPath, '/');

	if (ptr) {
		size_t length = ptr - _currentPath;

		__builtin_memcpy(selectedPath, _currentPath, length);
		selectedPath[length] = 0;
	} else {
		ptr      = __builtin_strchr(_currentPath, fs::VFS_PREFIX_SEPARATOR);
		*(++ptr) = 0;
	}
}

void FileBrowserScreen::_setPathToChild(const char *entry) {
	size_t length = __builtin_strlen(_currentPath);

	if (length) {
		__builtin_memcpy(selectedPath, _currentPath, length);
		selectedPath[length++] = '/';
	}

	__builtin_strncpy(
		&selectedPath[length], entry, sizeof(selectedPath) - length
	);
}

void FileBrowserScreen::_unloadDirectory(void) {
	_listLength     = 0;
	_numFiles       = 0;
	_numDirectories = 0;

	_files.destroy();
	_directories.destroy();
}

const char *FileBrowserScreen::_getItemName(ui::Context &ctx, int index) const {
	static char name[fs::MAX_NAME_LENGTH]; // TODO: get rid of this ugly crap

	if (!_isRoot)
		index--;

	const char *format, *path;

	if (index < 0) {
		format = CH_PARENT_DIR_ICON " %s";
		path   = STR("FileBrowserScreen.parentDir");
	} else if (index < _numDirectories) {
		auto entries = _directories.as<fs::FileInfo>();

		format = CH_DIR_ICON " %s";
		path   = entries[index].name;
	} else {
		auto entries = _files.as<fs::FileInfo>();

		format = CH_FILE_ICON " %s";
		path   = entries[index - _numDirectories].name;
	}

	snprintf(name, sizeof(name), format, path);
	return name;
}

int FileBrowserScreen::loadDirectory(
	ui::Context &ctx, const char *path, bool updateCurrent
) {
	_unloadDirectory();

	// Count the number of files and subfolders in the current directory, so
	// that we can allocate enough space for them.
	auto directory = APP->_fileIO.vfs.openDirectory(path);

	if (!directory)
		return -1;

	fs::FileInfo info;

	while (directory->getEntry(info)) {
		if (info.attributes & fs::DIRECTORY)
			_numDirectories++;
		else
			_numFiles++;
	}

	directory->close();
	delete directory;

	_activeItem = 0;
	_listLength = _numFiles + _numDirectories;
	_isRoot     = bool(!__builtin_strchr(path, '/'));

	if (!_isRoot)
		_listLength++;

	LOG_APP("files=%d, dirs=%d", _numFiles, _numDirectories);

	fs::FileInfo *files       = nullptr;
	fs::FileInfo *directories = nullptr;

	if (_numFiles)
		files       = _files.allocate<fs::FileInfo>(_numFiles);
	if (_numDirectories)
		directories = _directories.allocate<fs::FileInfo>(_numDirectories);

	// Iterate over all entries again to populate the newly allocated arrays.
	directory = APP->_fileIO.vfs.openDirectory(path);

	if (!directory)
		return -1;

	while (directory->getEntry(info)) {
		auto ptr = (info.attributes & fs::DIRECTORY)
			? (directories++) : (files++);

		__builtin_memcpy(ptr, &info, sizeof(fs::FileInfo));
	}

	directory->close();
	delete directory;

	if (updateCurrent)
		__builtin_strncpy(_currentPath, path, sizeof(_currentPath));

	return _numFiles + _numDirectories;
}

void FileBrowserScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("FileBrowserScreen.title");
	_prompt     = APP->_filePickerScreen._promptText;
	_itemPrompt = STR("FileBrowserScreen.itemPrompt");

	ListScreen::show(ctx, goBack);
}

void FileBrowserScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT)) {
			ctx.show(APP->_filePickerScreen, true, true);
		} else {
			int index = _activeItem;

			if (!_isRoot)
				index--;

			if (index < _numDirectories) {
				auto entries = _directories.as<fs::FileInfo>();

				if (index < 0)
					_setPathToParent();
				else
					_setPathToChild(entries[index].name);

				if (loadDirectory(ctx, selectedPath) < 0) {
					loadDirectory(ctx, _currentPath, false);

					APP->_messageScreen.previousScreens[MESSAGE_ERROR] = this;
					APP->_messageScreen.setMessage(
						MESSAGE_ERROR, STR("FileBrowserScreen.subdirError"),
						selectedPath
					);
					ctx.show(APP->_messageScreen, false, true);
				}
			} else {
				auto entries = _files.as<fs::FileInfo>();

				_setPathToChild(entries[index - _numDirectories].name);
				APP->_filePickerScreen._callback(ctx);
			}
		}
	}
}
