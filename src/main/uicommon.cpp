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

#include "common/util/templates.hpp"
#include "common/defs.hpp"
#include "common/gpu.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"
#include "ps1/gpucmd.h"

namespace ui {

/* Common screens */

TextScreen::TextScreen(void)
: _title(nullptr), _body(nullptr), _prompt(nullptr) {}

void TextScreen::show(Context &ctx, bool goBack) {
	AnimatedScreen::show(ctx, goBack);

	_scrollAnim.setValue(0);
	_updateTextHeight(ctx);
}

void TextScreen::draw(Context &ctx, bool active) const {
	int screenWidth  = ctx.gpuCtx.width  - SCREEN_MARGIN_X * 2;
	int screenHeight = ctx.gpuCtx.height - SCREEN_MARGIN_Y * 2;
	int lineHeight   = ctx.font.getLineHeight();

	// Top/bottom text
	_newLayer(
		ctx, SCREEN_MARGIN_X, SCREEN_MARGIN_Y, screenWidth, screenHeight
	);

	gpu::Rect rect;

	rect.x1 = 0;
	rect.y1 = 0;
	rect.x2 = screenWidth;
	rect.y2 = lineHeight;
	ctx.font.draw(ctx.gpuCtx, _title, rect, ctx.colors[COLOR_TITLE]);

	rect.y1 = screenHeight - SCREEN_PROMPT_HEIGHT_MIN;
	rect.y2 = screenHeight;
	ctx.font.draw(ctx.gpuCtx, _prompt, rect, ctx.colors[COLOR_TEXT1], true);

	int bodyOffset = lineHeight + SCREEN_BLOCK_MARGIN;
	int bodyHeight = screenHeight -
		(bodyOffset + SCREEN_PROMPT_HEIGHT_MIN + SCREEN_BLOCK_MARGIN);

	// Scrollable text
	_newLayer(
		ctx, SCREEN_MARGIN_X, SCREEN_MARGIN_Y + bodyOffset, screenWidth,
		bodyHeight
	);

	gpu::Rect clip;

	rect.y1 = -_scrollAnim.getValue(ctx.time);
	rect.y2 = 0x7fff;
	clip.x1 = 0;
	clip.y1 = 0;
	clip.x2 = screenWidth;
	clip.y2 = bodyHeight;
	ctx.font.draw(ctx.gpuCtx, _body, rect, clip, ctx.colors[COLOR_TEXT1], true);
}

void TextScreen::update(Context &ctx) {
	if (!ctx.buttons.held(ui::BTN_LEFT) && !ctx.buttons.held(ui::BTN_RIGHT))
		return;

	int screenHeight = ctx.gpuCtx.height - SCREEN_MARGIN_Y * 2;
	int bodyOffset   = ctx.font.getLineHeight() + SCREEN_BLOCK_MARGIN;
	int bodyHeight   = screenHeight -
		(bodyOffset + SCREEN_PROMPT_HEIGHT_MIN + SCREEN_BLOCK_MARGIN);

	int scrollHeight = _textHeight - util::min(_textHeight, bodyHeight);

	int oldValue = _scrollAnim.getTargetValue();
	int value    = oldValue;

	if (
		ctx.buttons.pressed(ui::BTN_LEFT) ||
		(ctx.buttons.longHeld(ui::BTN_LEFT) && (value > 0))
	) {
		if (value <= 0) {
			value = scrollHeight;
			ctx.sounds[SOUND_CLICK].play();
		} else {
			value -= util::min(SCROLL_AMOUNT, value);
			ctx.sounds[SOUND_MOVE].play();
		}

		_scrollAnim.setValue(ctx.time, oldValue, value, SPEED_FASTEST);
	}
	if (
		ctx.buttons.pressed(ui::BTN_RIGHT) ||
		(ctx.buttons.longHeld(ui::BTN_RIGHT) && (value < scrollHeight))
	) {
		if (value >= scrollHeight) {
			value = 0;
			ctx.sounds[SOUND_CLICK].play();
		} else {
			value += util::min(SCROLL_AMOUNT, scrollHeight - value);
			ctx.sounds[SOUND_MOVE].play();
		}

		_scrollAnim.setValue(ctx.time, oldValue, value, SPEED_FASTEST);
	}
}

ImageScreen::ImageScreen(void)
: _image(nullptr), _imageScale(1), _imagePadding(0), _title(nullptr),
_prompt(nullptr) {}

void ImageScreen::draw(Context &ctx, bool active) const {
	_newLayer(ctx, 0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height);

	int lineHeight = ctx.font.getLineHeight();

	if (_image) {
		int x      = ctx.gpuCtx.width  / 2;
		int y      = ctx.gpuCtx.height / 2;
		int width  = _image->width  * _imageScale / 2;
		int height = _image->height * _imageScale / 2;

		if (_prompt)
			y -= (SCREEN_PROMPT_HEIGHT - lineHeight) / 2;

		// Backdrop
		if (_imagePadding) {
			int _width  = width  + _imagePadding;
			int _height = height + _imagePadding;

			ctx.gpuCtx.drawRect(
				x - _width, y - _height, _width * 2, _height * 2, _backdropColor
			);
		}

		// Image
		if (_imageScale > 1)
			_image->drawScaled(
				ctx.gpuCtx, x - width - 1, y - height - 1, width * 2, height * 2
			);
		else
			_image->draw(ctx.gpuCtx, x - width, y - height);
	}

	// Text
	gpu::Rect rect;

	rect.x1 = SCREEN_MARGIN_X;
	rect.y1 = SCREEN_MARGIN_Y;
	rect.x2 = ctx.gpuCtx.width - SCREEN_MARGIN_X;
	rect.y2 = SCREEN_MARGIN_Y + lineHeight;
	ctx.font.draw(ctx.gpuCtx, _title, rect, ctx.colors[COLOR_TITLE]);

	rect.y1 = ctx.gpuCtx.height - (SCREEN_MARGIN_Y + SCREEN_PROMPT_HEIGHT);
	rect.y2 = ctx.gpuCtx.height - SCREEN_MARGIN_Y;
	ctx.font.draw(ctx.gpuCtx, _prompt, rect, ctx.colors[COLOR_TEXT1], true);
}

ListScreen::ListScreen(void)
: _listLength(0), _prompt(nullptr) {}

void ListScreen::_drawItems(Context &ctx) const {
	int itemY      = _scrollAnim.getValue(ctx.time);
	int itemWidth  = _getItemWidth(ctx);
	int listHeight = _getListHeight(ctx);
	int lineHeight = ctx.font.getLineHeight();

	gpu::Rect rect;

	rect.x1 = LIST_BOX_PADDING + LIST_ITEM_PADDING;
	rect.x2 = itemWidth - LIST_ITEM_PADDING;
	//rect.y2 = listHeight;

	for (int i = 0; (i < _listLength) && (itemY < listHeight); i++) {
		int itemHeight = lineHeight + LIST_ITEM_PADDING * 2;

		if (i == _activeItem)
			itemHeight += lineHeight;

		if ((itemY + itemHeight) >= 0) {
			if (i == _activeItem) {
				ctx.gpuCtx.drawRect(
					LIST_BOX_PADDING, itemY, itemWidth, itemHeight,
					ctx.colors[COLOR_HIGHLIGHT2]
				);
				ctx.gpuCtx.drawRect(
					LIST_BOX_PADDING, itemY, _itemAnim.getValue(ctx.time),
					itemHeight, ctx.colors[COLOR_HIGHLIGHT1]
				);

				rect.y1 = itemY + LIST_ITEM_PADDING + lineHeight;
				rect.y2 = rect.y1 + lineHeight;
				ctx.font.draw(
					ctx.gpuCtx, _itemPrompt, rect, ctx.colors[COLOR_SUBTITLE]
				);
			}

			rect.y1 = itemY + LIST_ITEM_PADDING;
			rect.y2 = rect.y1 + lineHeight;
			ctx.font.draw(
				ctx.gpuCtx, _getItemName(ctx, i), rect, ctx.colors[COLOR_TITLE]
			);
		}

		itemY += itemHeight;
	}
}

void ListScreen::show(Context &ctx, bool goBack) {
	AnimatedScreen::show(ctx, goBack);

	_activeItem = 0;
	_scrollAnim.setValue(LIST_BOX_PADDING);
	_itemAnim.setValue(_getItemWidth(ctx));
}

void ListScreen::draw(Context &ctx, bool active) const {
	int screenWidth  = ctx.gpuCtx.width  - SCREEN_MARGIN_X * 2;
	int screenHeight = ctx.gpuCtx.height - SCREEN_MARGIN_Y * 2;
	int listHeight   = _getListHeight(ctx);
	int lineHeight   = ctx.font.getLineHeight();

	_newLayer(
		ctx, SCREEN_MARGIN_X, SCREEN_MARGIN_Y, screenWidth, screenHeight
	);

	// Text
	gpu::Rect rect;

	rect.x1 = 0;
	rect.y1 = 0;
	rect.x2 = screenWidth;
	rect.y2 = lineHeight;
	ctx.font.draw(ctx.gpuCtx, _title, rect, ctx.colors[COLOR_TITLE]);

	rect.y1 = screenHeight - SCREEN_PROMPT_HEIGHT;
	rect.y2 = screenHeight;
	ctx.font.draw(ctx.gpuCtx, _prompt, rect, ctx.colors[COLOR_TEXT1], true);

	_newLayer(
		ctx, SCREEN_MARGIN_X,
		SCREEN_MARGIN_Y + lineHeight + SCREEN_BLOCK_MARGIN, screenWidth,
		listHeight
	);
	_setBlendMode(ctx, GP0_BLEND_SEMITRANS, true);

	// List box
	ctx.gpuCtx.drawRect(
		0, 0, screenWidth / 2, listHeight, ctx.colors[COLOR_BOX1]
	);
	ctx.gpuCtx.drawGradientRectH(
		screenWidth / 2, 0, screenWidth / 2, listHeight, ctx.colors[COLOR_BOX1],
		ctx.colors[COLOR_BOX2]
	);

	if (_listLength) {
		_drawItems(ctx);

		// Up/down arrow icons
		gpu::RectWH iconRect;

		iconRect.x = screenWidth - (lineHeight + LIST_BOX_PADDING);
		iconRect.w = lineHeight;
		iconRect.h = lineHeight;

		if (_activeItem) {
			iconRect.y = LIST_BOX_PADDING;
			ctx.font.draw(
				ctx.gpuCtx, CH_UP_ARROW, iconRect, ctx.colors[COLOR_TEXT1]
			);
		}
		if (_activeItem < (_listLength - 1)) {
			iconRect.y = listHeight - (lineHeight + LIST_BOX_PADDING);
			ctx.font.draw(
				ctx.gpuCtx, CH_DOWN_ARROW, iconRect, ctx.colors[COLOR_TEXT1]
			);
		}
	}
}

void ListScreen::update(Context &ctx) {
	if (
		ctx.buttons.pressed(ui::BTN_LEFT) ||
		(ctx.buttons.longHeld(ui::BTN_LEFT) && (_activeItem > 0))
	) {
		_activeItem--;
		if (_activeItem < 0) {
			_activeItem += _listLength;
			ctx.sounds[SOUND_CLICK].play();
		} else {
			ctx.sounds[SOUND_MOVE].play();
		}

		_itemAnim.setValue(ctx.time, 0, _getItemWidth(ctx), SPEED_FAST);

	}
	if (
		ctx.buttons.pressed(ui::BTN_RIGHT) ||
		(ctx.buttons.longHeld(ui::BTN_RIGHT) && (_activeItem < (_listLength - 1)))
	) {
		_activeItem++;
		if (_activeItem >= _listLength) {
			_activeItem -= _listLength;
			ctx.sounds[SOUND_CLICK].play();
		} else {
			ctx.sounds[SOUND_MOVE].play();
		}

		_itemAnim.setValue(ctx.time, 0, _getItemWidth(ctx), SPEED_FAST);
	}

	// Scroll the list if the selected item is not fully visible.
	int lineHeight       = ctx.font.getLineHeight();
	int itemHeight       = lineHeight + LIST_ITEM_PADDING * 2;
	int activeItemHeight = lineHeight + itemHeight;

	int topOffset     = _activeItem * itemHeight;
	int bottomOffset  = topOffset + activeItemHeight - _getListHeight(ctx);
	int currentOffset = -_scrollAnim.getTargetValue();

	if (topOffset < currentOffset)
		_scrollAnim.setValue(ctx.time, LIST_BOX_PADDING - topOffset, SPEED_FAST);
	else if (bottomOffset > currentOffset)
		_scrollAnim.setValue(ctx.time, -(LIST_BOX_PADDING + bottomOffset), SPEED_FAST);
}

}
