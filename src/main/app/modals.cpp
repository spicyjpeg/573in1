
#include <stdarg.h>
#include <stdio.h>
#include "common/defs.hpp"
#include "common/file.hpp"
#include "common/filemisc.hpp"
#include "common/ide.hpp"
#include "common/util.hpp"
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
	auto &worker    = APP->_workerStatus;
	auto nextScreen = worker.nextScreen;

	if ((worker.status == WORKER_NEXT) || (worker.status == WORKER_NEXT_BACK)) {
		worker.reset();
		ctx.show(*nextScreen, worker.status == WORKER_NEXT_BACK);

		LOG("worker finished, next=0x%08x", nextScreen);
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

void MessageScreen::setMessage(
	MessageType type, ui::Screen &prev, const char *format, ...
) {
	_type       = type;
	_prevScreen = &prev;

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
	_locked     = _prevScreen ? false : true;

	MessageBoxScreen::show(ctx, goBack);
	ctx.sounds[ui::SOUND_ALERT].play();
}

void MessageScreen::update(ui::Context &ctx) {
	MessageBoxScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(*_prevScreen, true, true);
}

void ConfirmScreen::setMessage(
	ui::Screen &prev, void (*callback)(ui::Context &ctx), const char *format,
	...
) {
	_prevScreen = &prev;
	_callback   = callback;

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
	//ctx.sounds[ui::SOUND_ALERT].play();
}

void ConfirmScreen::update(ui::Context &ctx) {
	MessageBoxScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (_activeButton)
			_callback(ctx);
		else
			ctx.show(*_prevScreen, true, true);
	}
}

/* File picker screen */

const char *FilePickerScreen::_getItemName(ui::Context &ctx, int index) const {
	static char name[file::MAX_NAME_LENGTH]; // TODO: get rid of this ugly crap

	int  drive = _drives[index];
	auto &dev  = ide::devices[drive];
	auto fs    = APP->_fileIO.ide[drive];

	if (dev.flags & ide::DEVICE_ATAPI)
		name[0] = CH_CDROM_ICON;
	else
		name[0] = CH_HDD_ICON;

	name[1] = ' ';

	if (fs)
		snprintf(
			&name[2], sizeof(name) - 2, "%s: %s", dev.model, fs->volumeLabel
		);
	else
		__builtin_strncpy(&name[2], dev.model, sizeof(name) - 2);

	return name;
}

void FilePickerScreen::setMessage(
	ui::Screen &prev, void (*callback)(ui::Context &ctx), const char *format,
	...
) {
	_prevScreen = &prev;
	_callback   = callback;

	va_list ap;

	va_start(ap, format);
	vsnprintf(_promptText, sizeof(_promptText), format, ap);
	va_end(ap);
}

int FilePickerScreen::loadRootAndShow(ui::Context &ctx) {
	_listLength = 0;

	for (size_t i = 0; i < util::countOf(ide::devices); i++) {
		if (ide::devices[i].flags & ide::DEVICE_READY)
			_drives[_listLength++] = i;
	}

	if (_listLength) {
		ctx.show(APP->_filePickerScreen, false, true);
	} else {
		APP->_messageScreen.setMessage(
			MESSAGE_ERROR, *_prevScreen, STR("FilePickerScreen.noDeviceError")
		);
		ctx.show(APP->_messageScreen, false, true);
	}

	return _listLength;
}

void FilePickerScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("FilePickerScreen.title");
	_prompt     = _promptText;
	_itemPrompt = STR("FilePickerScreen.itemPrompt");

	ListScreen::show(ctx, goBack);
}

void FilePickerScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT)) {
			ctx.show(*_prevScreen, true, true);
		} else {
			char name[6]{ "ide#:" };

			int  drive = _drives[_activeItem];
			auto &dev  = ide::devices[drive];

			name[3]   = drive + '0';
			int count = APP->_fileBrowserScreen.loadDirectory(ctx, name);

			if (count > 0) {
				ctx.show(APP->_fileBrowserScreen, false, true);
			} else {
				util::Hash error;

				if (!count)
					error = "FilePickerScreen.noFilesError"_h;
				else if (dev.flags & ide::DEVICE_ATAPI)
					error = "FilePickerScreen.atapiError"_h;
				else
					error = "FilePickerScreen.ideError"_h;

				APP->_messageScreen.setMessage(
					MESSAGE_ERROR, *this, STRH(error)
				);
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
		ptr      = __builtin_strchr(_currentPath, file::VFS_PREFIX_SEPARATOR);
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
	_numFiles       = 0;
	_numDirectories = 0;

	_files.destroy();
	_directories.destroy();
}

const char *FileBrowserScreen::_getItemName(ui::Context &ctx, int index) const {
	static char name[file::MAX_NAME_LENGTH]; // TODO: get rid of this ugly crap

	if (!_isRoot)
		index--;

	const char *path;

	if (index < 0) {
		name[0] = CH_PARENT_DIR_ICON;
		path    = STR("FileBrowserScreen.parentDir");
	} else if (index < _numDirectories) {
		auto entries = _directories.as<file::FileInfo>();

		name[0] = CH_DIR_ICON;
		path    = entries[index].name;
	} else {
		auto entries = _files.as<file::FileInfo>();

		name[0] = CH_FILE_ICON;
		path    = entries[index - _numDirectories].name;
	}

	name[1] = ' ';
	__builtin_strncpy(&name[2], path, sizeof(name) - 2);

	return name;
}

int FileBrowserScreen::loadDirectory(ui::Context &ctx, const char *path) {
	_unloadDirectory();

	// Count the number of files and subfolders in the current directory, so
	// that we can allocate enough space for them.
	auto directory = APP->_fileIO.vfs.openDirectory(path);

	if (!directory)
		return -1;

	file::FileInfo info;

	while (directory->getEntry(info)) {
		if (info.attributes & file::DIRECTORY)
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

	LOG("files=%d, dirs=%d", _numFiles, _numDirectories);

	file::FileInfo *files, *directories;

	if (_numFiles)
		files       = _files.allocate<file::FileInfo>(_numFiles);
	if (_numDirectories)
		directories = _directories.allocate<file::FileInfo>(_numDirectories);

	// Iterate over all entries again to populate the newly allocated arrays.
	directory = APP->_fileIO.vfs.openDirectory(path);

	if (!directory)
		return -1;

	while (directory->getEntry(info)) {
		auto ptr = (info.attributes & file::DIRECTORY)
			? (directories++) : (files++);

		__builtin_memcpy(ptr, &info, sizeof(file::FileInfo));
	}

	directory->close();
	delete directory;

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
			_unloadDirectory();
			ctx.show(APP->_filePickerScreen, true, true);
		} else {
			int index = _activeItem;

			if (!_isRoot)
				index--;

			if (index < _numDirectories) {
				auto entries = _directories.as<file::FileInfo>();

				if (index < 0)
					_setPathToParent();
				else
					_setPathToChild(entries[index].name);

				if (loadDirectory(ctx, selectedPath) < 0) {
					APP->_messageScreen.setMessage(
						MESSAGE_ERROR, *this,
						STR("FileBrowserScreen.subdirError"), selectedPath
					);

					ctx.show(APP->_messageScreen, false, true);
				}
			} else {
				auto entries = _files.as<file::FileInfo>();

				_setPathToChild(entries[index - _numDirectories].name);
				APP->_filePickerScreen._callback(ctx);
			}
		}
	}
}
