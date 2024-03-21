
#include <stdarg.h>
#include <stdio.h>
#include "common/defs.hpp"
#include "common/file.hpp"
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

void FilePickerScreen::_setPathToParent(void) {
	auto ptr = __builtin_strrchr(_currentPath, '/');

	if (ptr) {
		size_t length = ptr - _currentPath;

		__builtin_memcpy(selectedPath, _currentPath, length);
		selectedPath[length] = 0;
	} else {
		selectedPath[0] = 0;
	}
}

void FilePickerScreen::_setPathToChild(const char *entry) {
	size_t length = __builtin_strlen(_currentPath);

	if (length) {
		__builtin_memcpy(selectedPath, _currentPath, length);
		selectedPath[length++] = '/';
	}

	__builtin_strncpy(
		&selectedPath[length], entry, sizeof(selectedPath) - length
	);
}

void FilePickerScreen::_unloadDirectory(void) {
	_numFiles       = 0;
	_numDirectories = 0;
	_files.destroy();
	_directories.destroy();
}

const char *FilePickerScreen::_getItemName(ui::Context &ctx, int index) const {
	static char name[file::MAX_NAME_LENGTH]; // TODO: get rid of this ugly crap

	if (_currentPath[0])
		index--;

	const char *path;
	char       icon;

	if (index < 0) {
		path = STR("FilePickerScreen.parentDir");
		icon = CH_PARENT_DIR_ICON;
	} else if (index < _numDirectories) {
		auto entries = _directories.as<file::FileInfo>();

		path = entries[index].name;
		icon = CH_DIR_ICON;
	} else {
		auto entries = _files.as<file::FileInfo>();

		path = entries[index - _numDirectories].name;
		icon = CH_FILE_ICON;
	}

	name[0] = icon;
	name[1] = ' ';
	__builtin_strncpy(&name[2], path, sizeof(name) - 2);

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

int FilePickerScreen::loadDirectory(ui::Context &ctx, const char *path) {
	_unloadDirectory();

	// Count the number of files and subfolders in the current directory, so
	// that we can allocate enough space for them.
	auto directory = APP->_fileProvider.openDirectory(path);
	if (!directory)
		return -1;

	file::FileInfo info;

	while (directory->getEntry(info)) {
		if (info.attributes & file::DIRECTORY)
			_numDirectories++;
		else
			_numFiles++;
	}

	delete directory;

	_activeItem = 0;
	_listLength = _numFiles + _numDirectories;
	if (path[0])
		_listLength++;

	LOG("path: %s", path);
	LOG("files=%d, dirs=%d", _numFiles, _numDirectories);

	if (_numFiles)
		_files.allocate(sizeof(file::FileInfo) * _numFiles);
	if (_numDirectories)
		_directories.allocate(sizeof(file::FileInfo) * _numDirectories);

	auto files       = _files.as<file::FileInfo>();
	auto directories = _directories.as<file::FileInfo>();

	// Iterate over all entries again to populate the newly allocated arrays.
	directory = APP->_fileProvider.openDirectory(path);
	if (!directory)
		return -1;

	while (directory->getEntry(info)) {
		auto ptr = (info.attributes & file::DIRECTORY)
			? (directories++) : (files++);

		__builtin_memcpy(ptr, &info, sizeof(file::FileInfo));
	}

	delete directory;

	__builtin_strncpy(_currentPath, path, sizeof(_currentPath));
	return _numFiles + _numDirectories;
}

int FilePickerScreen::loadRootAndShow(ui::Context &ctx) {
	int count = loadDirectory(ctx, "");

	if (count > 0) {
		ctx.show(*this, false, true);
	} else {
		auto error = (count < 0)
			? "FilePickerScreen.rootError"_h
			: "FilePickerScreen.noFilesError"_h;

		APP->_messageScreen.setMessage(MESSAGE_ERROR, *this, STRH(error));
		ctx.show(APP->_messageScreen, false, true);
	}

	return count;
}

void FilePickerScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("FilePickerScreen.title");
	_prompt     = _promptText;
	_itemPrompt = STR("FilePickerScreen.itemPrompt");

	//loadDirectory(ctx, "");

	ListScreen::show(ctx, goBack);
}

void FilePickerScreen::hide(ui::Context &ctx, bool goBack) {
	//_unloadDirectory();

	ListScreen::hide(ctx, goBack);
}

void FilePickerScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT)) {
			_unloadDirectory();
			ctx.show(*_prevScreen, true, true);
		} else {
			int index = _activeItem;

			if (_currentPath[0])
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
						STR("FilePickerScreen.subdirError"), selectedPath
					);

					ctx.show(APP->_messageScreen, false, true);
				}
			} else {
				auto entries = _files.as<file::FileInfo>();

				_setPathToChild(entries[index - _numDirectories].name);
				_callback(ctx);
			}
		}
	}
}
