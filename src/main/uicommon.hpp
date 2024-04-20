
#pragma once

#include <stdint.h>
#include "common/gpu.hpp"
#include "common/util.hpp"
#include "main/uibase.hpp"

namespace ui {

/* Common screens */

class PlaceholderScreen : public AnimatedScreen {
public:
	void draw(Context &ctx, bool active) const;
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
		return screenHeight - (
			ctx.font.metrics.lineHeight + SCREEN_PROMPT_HEIGHT +
			SCREEN_BLOCK_MARGIN * 2
		);
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
