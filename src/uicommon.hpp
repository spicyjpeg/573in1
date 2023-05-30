
#pragma once

#include "uibase.hpp"

namespace ui {

/* Common higher-level screens */

class PlaceholderScreen : public AnimatedScreen {
public:
	void draw(Context &ctx, bool active) const;
};

class MessageScreen : public ModalScreen {
private:
	util::Tween<int, util::QuadOutEasing> _buttonAnim;

	inline int _getButtonWidth(void) const {
		return ((_width / 4) * 3) / _numButtons - BUTTON_SPACING;
	}

protected:
	int  _numButtons, _activeButton;
	bool _locked;

	const char *_buttons[3];

public:
	MessageScreen(void);
	virtual void show(Context &ctx, bool goBack = false);
	virtual void draw(Context &ctx, bool active = true) const;
	virtual void update(Context &ctx);
};

class ProgressScreen : public ModalScreen {
private:
	util::Tween<int, util::QuadOutEasing> _progressBarAnim;

protected:
	inline void _setProgress(Context &ctx, int part, int total) {
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
protected:
	const char *_title, *_body, *_prompt;

public:
	TextScreen(void);
	virtual void draw(Context &ctx, bool active = true) const;
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

class HexEntryScreen : public AnimatedScreen {
protected:
	uint8_t _buffer[16];

	const char *_title, *_prompt;

public:
	HexEntryScreen(void);
	virtual void draw(Context &ctx, bool active = true) const;
	virtual void update(Context &ctx);
};

}
