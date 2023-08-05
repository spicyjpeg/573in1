
#pragma once

#include <stdint.h>
#include "gpufont.hpp"
#include "uibase.hpp"
#include "util.hpp"

namespace ui {

/* Common higher-level screens */

class PlaceholderScreen : public AnimatedScreen {
public:
	void draw(Context &ctx, bool active) const;
};

class MessageBoxScreen : public ModalScreen {
private:
	util::Tween<int, util::QuadOutEasing> _buttonAnim;

	inline int _getButtonWidth(void) const {
		return ((_width / 4) * 3) / _numButtons - BUTTON_SPACING;
	}

protected:
	int  _numButtons, _activeButton, _buttonIndexOffset;
	bool _locked;

	const char *_buttons[3];

public:
	MessageBoxScreen(void);
	virtual void show(Context &ctx, bool goBack = false);
	virtual void draw(Context &ctx, bool active = true) const;
	virtual void update(Context &ctx);
};

class HexEntryScreen : public MessageBoxScreen {
private:
	int _charIndex;

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

class TextScreen : public AnimatedScreen {
private:
	util::Tween<int, util::QuadOutEasing> _scrollAnim;

	int _textHeight;

protected:
	const char *_title, *_body, *_prompt;

	inline void _updateTextHeight(ui::Context &ctx) {
		int screenWidth = ctx.gpuCtx.width - SCREEN_MARGIN_X * 2;

		_textHeight = ctx.font.getStringHeight(_body, screenWidth, true);
	}

public:
	TextScreen(void);
	virtual void show(Context &ctx, bool goBack = false);
	virtual void draw(Context &ctx, bool active = true) const;
	virtual void update(Context &ctx);
};

class ImageScreen : public AnimatedScreen {
protected:
	gpu::Image _image;
	int        _imageScale, _imagePadding;
	gpu::Color _backdropColor;

	const char *_title, *_prompt;

public:
	ImageScreen(void);
	virtual void draw(Context &ctx, bool active = true) const;
};

class ListScreen : public AnimatedScreen {
private:
	util::Tween<int, util::QuadOutEasing> _scrollAnim, _itemAnim;

	inline int _getItemWidth(Context &ctx) const {
		return ctx.gpuCtx.width - (SCREEN_MARGIN_X + LIST_BOX_PADDING) * 2;
	}
	inline int _getListHeight(Context &ctx) const {
		int screenHeight = ctx.gpuCtx.height - SCREEN_MARGIN_Y * 2;
		return screenHeight -
			(gpu::FONT_LINE_HEIGHT + SCREEN_PROMPT_HEIGHT + SCREEN_BLOCK_MARGIN * 2);
	}

	void _drawItems(Context &ctx) const;

protected:
	int _listLength, _activeItem;

	const char *_title, *_prompt, *_itemPrompt;

	virtual const char *_getItemName(ui::Context &ctx, int index) const {
		return nullptr;
	}

public:
	ListScreen(void);
	virtual void show(Context &ctx, bool goBack = false);
	virtual void draw(Context &ctx, bool active = true) const;
	virtual void update(Context &ctx);
};

}
