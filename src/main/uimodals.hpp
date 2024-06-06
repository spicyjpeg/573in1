
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
