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

#include <stdint.h>
#include "common/util.hpp"
#include "main/uibase.hpp"

namespace ui {

/* Common modal screens */

class MessageBoxScreen : public ModalScreen {
private:
	util::Tween<int, util::QuadOutEasing> _buttonAnim;

	inline int _getButtonWidth(void) const {
		return ((_width / 5) * 4) / _numButtons - BUTTON_SPACING;
	}

protected:
	int  _numButtons, _activeButton, _buttonIndexOffset;
	bool _locked;

	const char *_buttons[5];

public:
	MessageBoxScreen(void);
	virtual void show(Context &ctx, bool goBack = false);
	virtual void draw(Context &ctx, bool active = true) const;
	virtual void update(Context &ctx);
};

class HexEntryScreen : public MessageBoxScreen {
private:
	uint8_t _charWidth, _separatorWidth, _stringWidth;

	util::Tween<int, util::QuadOutEasing> _cursorAnim;

protected:
	uint8_t _buffer[32];
	char    _separator;

	int _bufferLength;

public:
	HexEntryScreen(void);
	virtual void show(Context &ctx, bool goBack = false);
	virtual void draw(Context &ctx, bool active = true) const;
	virtual void update(Context &ctx);
};

class DateEntryScreen : public MessageBoxScreen {
private:
	uint8_t _charWidth, _stringWidth, _fieldOffsets[6];

	util::Tween<int, util::QuadOutEasing> _cursorAnim;

protected:
	util::Date _date;

public:
	DateEntryScreen(void);
	virtual void show(Context &ctx, bool goBack = false);
	virtual void draw(Context &ctx, bool active = true) const;
	virtual void update(Context &ctx);
};

class ProgressScreen : public ModalScreen {
private:
	util::Tween<int, util::QuadOutEasing> _progressBarAnim;

protected:
	inline void _setProgress(Context &ctx, int part, int total) {
		if (!total)
			total = 1;

		int totalWidth = _width - MODAL_PADDING * 2;
		int partWidth  = (totalWidth * part) / total;

		if (_progressBarAnim.getTargetValue() != partWidth)
			_progressBarAnim.setValue(ctx.time, partWidth, SPEED_FASTEST);
	}

public:
	ProgressScreen(void);
	virtual void show(Context &ctx, bool goBack = false);
	virtual void draw(Context &ctx, bool active = true) const;
};

}
