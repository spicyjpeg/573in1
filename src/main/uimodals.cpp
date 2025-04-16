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

#include <stddef.h>
#include <stdint.h>
#include "common/util/misc.hpp"
#include "common/util/string.hpp"
#include "common/util/templates.hpp"
#include "common/util/tween.hpp"
#include "common/gpu.hpp"
#include "main/uibase.hpp"
#include "main/uimodals.hpp"

namespace ui {

MessageBoxScreen::MessageBoxScreen(void)
: ModalScreen(MODAL_WIDTH, MODAL_HEIGHT_FULL), _numButtons(0),
_buttonIndexOffset(0), _locked(false) {}

void MessageBoxScreen::show(Context &ctx, bool goBack) {
	ModalScreen::show(ctx, goBack);

	_activeButton = 0;
	_buttonAnim.setValue(_getButtonWidth());
}

void MessageBoxScreen::draw(Context &ctx, bool active) const {
	ModalScreen::draw(ctx, active);

	if (!active || !_numButtons)
		return;

	int activeButton = _activeButton - _buttonIndexOffset;

	int buttonX = _width / 8;
	int buttonY = TITLE_BAR_HEIGHT + _height - (BUTTON_HEIGHT + MODAL_PADDING);
	gpu::RectWH rect;

	rect.y = buttonY + BUTTON_PADDING;
	rect.w = _getButtonWidth();
	rect.h = rect.y + ctx.font.getLineHeight();

	for (int i = 0; i < _numButtons; i++) {
		rect.x = buttonX +
			(rect.w - ctx.font.getStringWidth(_buttons[i])) / 2;

		if (_locked) {
			ctx.gpuCtx.drawRect(
				buttonX,
				buttonY,
				rect.w,
				BUTTON_HEIGHT,
				ctx.colors[COLOR_SHADOW],
				true
			);

			ctx.font.draw(
				ctx.gpuCtx,
				_buttons[i],
				rect,
				ctx.colors[COLOR_TEXT2]
			);
		} else {
			if (i == activeButton) {
				ctx.gpuCtx.drawRect(
					buttonX,
					buttonY,
					rect.w,
					BUTTON_HEIGHT,
					ctx.colors[COLOR_HIGHLIGHT2]
				);
				ctx.gpuCtx.drawRect(
					buttonX,
					buttonY,
					_buttonAnim.getValue(ctx.time),
					BUTTON_HEIGHT,
					ctx.colors[COLOR_HIGHLIGHT1]
				);
			} else {
				ctx.gpuCtx.drawRect(
					buttonX,
					buttonY,
					rect.w,
					BUTTON_HEIGHT,
					ctx.colors[COLOR_WINDOW3]
				);
			}

			ctx.font.draw(
				ctx.gpuCtx,
				_buttons[i],
				rect,
				ctx.colors[COLOR_TITLE]
			);
		}

		buttonX += rect.w + BUTTON_SPACING;
	}
}

void MessageBoxScreen::update(Context &ctx) {
	if (_locked)
		return;

	int numButtons = _buttonIndexOffset + _numButtons;

	if (
		ctx.buttons.pressed(ui::BTN_LEFT) ||
		(ctx.buttons.longHeld(ui::BTN_LEFT) && (_activeButton > 0))
	) {
		_activeButton--;
		if (_activeButton < 0) {
			_activeButton += numButtons;
			ctx.sounds[SOUND_CLICK].play();
		} else {
			ctx.sounds[SOUND_MOVE].play();
		}

		_buttonAnim.setValue(ctx.time, 0, _getButtonWidth(), SPEED_FASTEST);
	}
	if (
		ctx.buttons.pressed(ui::BTN_RIGHT) ||
		(ctx.buttons.longHeld(ui::BTN_RIGHT) && (_activeButton < (numButtons - 1)))
	) {
		_activeButton++;
		if (_activeButton >= numButtons) {
			_activeButton -= numButtons;
			ctx.sounds[SOUND_CLICK].play();
		} else {
			ctx.sounds[SOUND_MOVE].play();
		}

		_buttonAnim.setValue(ctx.time, 0, _getButtonWidth(), SPEED_FASTEST);
	}
}

HexEntryScreen::HexEntryScreen(void)
: _bufferLength(0) {
	util::clear(_buffer);
}

void HexEntryScreen::show(Context &ctx, bool goBack) {
	MessageBoxScreen::show(ctx, goBack);

	_buttonIndexOffset = _bufferLength * 2;
	_charWidth         = ctx.font.getCharacterWidth('0');
	_separatorWidth    = ctx.font.getCharacterWidth(_separator);
	_stringWidth       = 0
		+ _charWidth      * (_bufferLength * 2)
		+ _separatorWidth * (_bufferLength - 1);

	_cursorAnim.setValue(0);
}

void HexEntryScreen::draw(Context &ctx, bool active) const {
	MessageBoxScreen::draw(ctx, active);

	if (!active)
		return;

	int boxY     =
		TITLE_BAR_HEIGHT + _height - (BUTTON_HEIGHT + MODAL_PADDING) * 2;
	int boxWidth = _width - MODAL_PADDING * 2;

	// Text box
	ctx.gpuCtx.drawRect(
		MODAL_PADDING,
		boxY,
		boxWidth,
		BUTTON_HEIGHT,
		ctx.colors[COLOR_BOX1]
	);

	char      string[128];
	gpu::Rect rect;

	util::hexToString(string, _buffer, _bufferLength, _separator);

	int stringOffset = MODAL_PADDING + (boxWidth - _stringWidth) / 2;
	int charIndex    = _activeButton + _activeButton / 2;

	// Cursor
	if (_activeButton < _buttonIndexOffset)
		ctx.gpuCtx.drawGradientRectV(
			stringOffset + _cursorAnim.getValue(ctx.time),
			boxY + BUTTON_HEIGHT / 2,
			_charWidth,
			BUTTON_HEIGHT / 2,
			ctx.colors[COLOR_BOX1],
			ctx.colors[COLOR_HIGHLIGHT1]
		);

	// Current string
	rect.x1 = stringOffset;
	rect.y1 = boxY + BUTTON_PADDING;
	rect.x2 = _width - MODAL_PADDING;
	rect.y2 = boxY + BUTTON_PADDING + ctx.font.getLineHeight();
	ctx.font.draw(
		ctx.gpuCtx,
		string,
		rect,
		ctx.colors[COLOR_TITLE]
	);

	// Highlighted field
	if (_activeButton < _buttonIndexOffset) {
		auto ptr = &string[charIndex];
		ptr[1]   = 0;

		rect.x1 = stringOffset + _cursorAnim.getTargetValue();
		ctx.font.draw(
			ctx.gpuCtx,
			ptr,
			rect,
			ctx.colors[COLOR_SUBTITLE]
		);
	}
}

void HexEntryScreen::update(Context &ctx) {
	if (
		ctx.buttons.held(ui::BTN_START) && (_activeButton < _buttonIndexOffset)
	) {
		auto ptr   = &_buffer[_activeButton / 2];
		int  value = (_activeButton % 2) ? (*ptr & 0x0f) : (*ptr >> 4);

		if (
			ctx.buttons.pressed(ui::BTN_LEFT) ||
			(ctx.buttons.longHeld(ui::BTN_LEFT) && (value > 0))
		) {
			if (--value < 0) {
				value = 0xf;
				ctx.sounds[SOUND_CLICK].play();
			} else {
				ctx.sounds[SOUND_MOVE].play();
			}
		}
		if (
			ctx.buttons.pressed(ui::BTN_RIGHT) ||
			(ctx.buttons.longHeld(ui::BTN_RIGHT) && (value < 0xf))
		) {
			if (++value > 0xf) {
				value = 0;
				ctx.sounds[SOUND_CLICK].play();
			} else {
				ctx.sounds[SOUND_MOVE].play();
			}
		}

		if (_activeButton % 2)
			*ptr = (*ptr & 0xf0) | value;
		else
			*ptr = (*ptr & 0x0f) | (value << 4);
	} else {
		int oldActive = _activeButton;

		MessageBoxScreen::update(ctx);

		// Update the cursor's position if necessary.
		if (oldActive != _activeButton) {
			int cursorX = 0
				+ _charWidth      * _activeButton
				+ _separatorWidth * (_activeButton / 2);

			_cursorAnim.setValue(ctx.time, cursorX, SPEED_FASTEST);
		}
	}
}

struct DateField {
public:
	size_t   offset;
	uint16_t minValue, maxValue;
};

static const DateField _DATE_FIELDS[]{
	{
		.offset   = offsetof(util::Date, year),
		.minValue = 1970,
		.maxValue = 2069
	}, {
		.offset   = offsetof(util::Date, month),
		.minValue =  1,
		.maxValue = 12
	}, {
		.offset   = offsetof(util::Date, day),
		.minValue =  1,
		.maxValue = 31
	}, {
		.offset   = offsetof(util::Date, hour),
		.minValue =  0,
		.maxValue = 23
	}, {
		.offset   = offsetof(util::Date, minute),
		.minValue =  0,
		.maxValue = 59
	}, {
		.offset   = offsetof(util::Date, second),
		.minValue =  0,
		.maxValue = 59
	}
};

DateEntryScreen::DateEntryScreen(void) {
	_date.year   = 2000;
	_date.month  = 1;
	_date.day    = 1;
	_date.hour   = 0;
	_date.minute = 0;
	_date.second = 0;
}

void DateEntryScreen::show(Context &ctx, bool goBack) {
	MessageBoxScreen::show(ctx, goBack);

	_buttonIndexOffset = 6;
	_charWidth         = ctx.font.getCharacterWidth('0');

	int dateSepWidth = ctx.font.getCharacterWidth('-');
	int spaceWidth   = ctx.font.getSpaceWidth();
	int timeSepWidth = ctx.font.getCharacterWidth(':');

	_fieldOffsets[0] = 0;
	_fieldOffsets[1] = _fieldOffsets[0] + _charWidth * 4 + dateSepWidth;
	_fieldOffsets[2] = _fieldOffsets[1] + _charWidth * 2 + dateSepWidth;
	_fieldOffsets[3] = _fieldOffsets[2] + _charWidth * 2 + spaceWidth;
	_fieldOffsets[4] = _fieldOffsets[3] + _charWidth * 2 + timeSepWidth;
	_fieldOffsets[5] = _fieldOffsets[4] + _charWidth * 2 + timeSepWidth;
	_stringWidth     = _fieldOffsets[5] + _charWidth * 2;

	_cursorAnim.setValue(0);
}

void DateEntryScreen::draw(Context &ctx, bool active) const {
	MessageBoxScreen::draw(ctx, active);

	if (!active)
		return;

	int boxY     =
		TITLE_BAR_HEIGHT + _height - (BUTTON_HEIGHT + MODAL_PADDING) * 2;
	int boxWidth = _width - MODAL_PADDING * 2;

	// Text box
	ctx.gpuCtx.drawRect(
		MODAL_PADDING,
		boxY,
		boxWidth,
		BUTTON_HEIGHT,
		ctx.colors[COLOR_BOX1]
	);

	char      string[24];
	gpu::Rect rect;

	_date.toString(string);

	int stringOffset = MODAL_PADDING + (boxWidth - _stringWidth) / 2;
	int charIndex    = _activeButton * 3;
	int fieldLength  = 2;

	// The first field (year) has 4 digits, while all others have 2.
	if (_activeButton)
		charIndex   += 2;
	else
		fieldLength += 2;

	// Cursor
	if (_activeButton < _buttonIndexOffset)
		ctx.gpuCtx.drawGradientRectV(
			stringOffset + _cursorAnim.getValue(ctx.time),
			boxY + BUTTON_HEIGHT / 2,
			_charWidth * fieldLength,
			BUTTON_HEIGHT / 2,
			ctx.colors[COLOR_BOX1],
			ctx.colors[COLOR_HIGHLIGHT1]
		);

	// Current string
	rect.x1 = stringOffset;
	rect.y1 = boxY + BUTTON_PADDING;
	rect.x2 = _width - MODAL_PADDING;
	rect.y2 = boxY + BUTTON_PADDING + ctx.font.getLineHeight();
	ctx.font.draw(
		ctx.gpuCtx,
		string,
		rect,
		ctx.colors[COLOR_TITLE]
	);

	// Highlighted field
	if (_activeButton < _buttonIndexOffset) {
		auto ptr         = &string[charIndex];
		ptr[fieldLength] = 0;

		rect.x1 = stringOffset + _cursorAnim.getTargetValue();
		ctx.font.draw(
			ctx.gpuCtx,
			ptr,
			rect,
			ctx.colors[COLOR_SUBTITLE]
		);
	}
}

void DateEntryScreen::update(Context &ctx) {
	if (
		ctx.buttons.held(ui::BTN_START) && (_activeButton < _buttonIndexOffset)
	) {
		auto &field = _DATE_FIELDS[_activeButton];
		int  value;

		// The year is the only 16-bit field.
		if (!_activeButton)
			value = _date.year;
		else
			value = *reinterpret_cast<uint8_t *>(
				uintptr_t(&_date) + field.offset
			);

		if (
			ctx.buttons.pressed(ui::BTN_LEFT) ||
			(ctx.buttons.longHeld(ui::BTN_LEFT) && (value > field.minValue))
		) {
			if (--value < field.minValue) {
				value = field.maxValue;
				ctx.sounds[SOUND_CLICK].play();
			} else {
				ctx.sounds[SOUND_MOVE].play();
			}
		}
		if (
			ctx.buttons.pressed(ui::BTN_RIGHT) ||
			(ctx.buttons.longHeld(ui::BTN_RIGHT) && (value < field.maxValue))
		) {
			if (++value > field.maxValue) {
				value = field.minValue;
				ctx.sounds[SOUND_CLICK].play();
			} else {
				ctx.sounds[SOUND_MOVE].play();
			}
		}

		if (!_activeButton)
			_date.year = value;
		else
			*reinterpret_cast<uint8_t *>(
				uintptr_t(&_date) + field.offset
			) = value;

		// The day field must be fixed up after any date change.
		int maxDayValue = _date.getMonthDayCount();

		if (_date.day > maxDayValue)
			_date.day = maxDayValue;
	} else {
		int oldActive = _activeButton;

		MessageBoxScreen::update(ctx);

		// Update the cursor's position if necessary.
		if (oldActive != _activeButton)
			_cursorAnim.setValue(
				ctx.time,
				_fieldOffsets[_activeButton],
				SPEED_FASTEST
			);
	}
}

ProgressScreen::ProgressScreen(void)
: ModalScreen(MODAL_WIDTH, MODAL_HEIGHT_REDUCED) {}

void ProgressScreen::_setProgress(Context &ctx, int part, int total) {
	if (total > 0) {
		int fullBarWidth  = _width - MODAL_PADDING * 2;
		int progressWidth = (fullBarWidth * part) / total;

		if (_progressBarAnim.getTargetValue() != progressWidth)
			_progressBarAnim.setValue(ctx.time, progressWidth, SPEED_FASTEST);
	} else {
		_progressBarAnim.setValue(-1);
	}
}

void ProgressScreen::show(Context &ctx, bool goBack) {
	ModalScreen::show(ctx, goBack);

	_progressBarAnim.setValue(-1);
}

void ProgressScreen::draw(Context &ctx, bool active) const {
	ModalScreen::draw(ctx, active);

	if (!active)
		return;

	int fullBarWidth  = _width - MODAL_PADDING * 2;
	int progressWidth = _progressBarAnim.getValue(ctx.time);

	if (progressWidth < 0)
		return;

	int barX = (_width - fullBarWidth) / 2;
	int barY =
		TITLE_BAR_HEIGHT + _height - (PROGRESS_BAR_HEIGHT + MODAL_PADDING);

	_setBlendMode(ctx, GP0_BLEND_SEMITRANS, true);

	ctx.gpuCtx.drawRect(
		barX,
		barY,
		fullBarWidth,
		PROGRESS_BAR_HEIGHT,
		ctx.colors[COLOR_WINDOW3]
	);
	ctx.gpuCtx.drawGradientRectH(
		barX,
		barY,
		progressWidth,
		PROGRESS_BAR_HEIGHT,
		ctx.colors[COLOR_PROGRESS2],
		ctx.colors[COLOR_PROGRESS1]
	);
}

}
