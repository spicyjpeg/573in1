
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
	//_locked     = !previousScreen;

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
	//ctx.sounds[ui::SOUND_ALERT].play();
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

#ifndef NDEBUG
struct SpecialEntry {
public:
	util::Hash name;
	const char *prefix;
};

static const SpecialEntry _SPECIAL_ENTRIES[]{
	{
		.name   = "FilePickerScreen.host"_h,
		.prefix = "host:"
	}
};
#endif

const char *FilePickerScreen::_getItemName(ui::Context &ctx, int index) const {
#ifndef NDEBUG
	int offset = util::countOf(_SPECIAL_ENTRIES);

	if (index < offset)
		return STRH(_SPECIAL_ENTRIES[index].name);
	else
		index -= offset;
#endif

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
	void (*callback)(ui::Context &ctx), const char *format, ...
) {
	_callback = callback;

	va_list ap;

	va_start(ap, format);
	vsnprintf(_promptText, sizeof(_promptText), format, ap);
	va_end(ap);
}

void FilePickerScreen::reloadAndShow(ui::Context &ctx) {
	// Check if any disc has been changed and reload all filesystems if
	// necessary.
	for (auto &dev : ide::devices) {
		if (!dev.atapiPoll())
			continue;

		APP->_messageScreen.previousScreens[MESSAGE_ERROR] = this;

		APP->_runWorker(&App::_ideInitWorker, *this, false, true);
		return;
	}

	ctx.show(*this, false, true);
}

void FilePickerScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("FilePickerScreen.title");
	_prompt     = _promptText;
	_itemPrompt = STR("FilePickerScreen.itemPrompt");

	_listLength = 0;

	for (size_t i = 0; i < util::countOf(ide::devices); i++) {
		if (ide::devices[i].flags & ide::DEVICE_READY)
			_drives[_listLength++] = i;
	}

#ifndef NDEBUG
	_listLength += util::countOf(_SPECIAL_ENTRIES);
#endif

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
			int index  = _activeItem;
#ifndef NDEBUG
			int offset = util::countOf(_SPECIAL_ENTRIES);

			if (index < offset) {
				APP->_fileBrowserScreen.loadDirectory(
					ctx, _SPECIAL_ENTRIES[index].prefix
				);
				ctx.show(APP->_fileBrowserScreen, false, true);
				return;
			} else {
				index -= offset;
			}
#endif

			char name[6]{ "ide#:" };

			int  drive = _drives[index];
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
	_listLength     = 0;
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

int FileBrowserScreen::loadDirectory(
	ui::Context &ctx, const char *path, bool updateCurrent
) {
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

	file::FileInfo *files       = nullptr;
	file::FileInfo *directories = nullptr;

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
				auto entries = _directories.as<file::FileInfo>();

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
				auto entries = _files.as<file::FileInfo>();

				_setPathToChild(entries[index - _numDirectories].name);
				APP->_filePickerScreen._callback(ctx);
			}
		}
	}
}
