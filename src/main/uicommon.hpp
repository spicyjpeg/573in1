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
#include "common/util/tween.hpp"
#include "common/gpu.hpp"
#include "main/uibase.hpp"

namespace ui {

/* Common screens */

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
	gpu::Image *_image;
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
