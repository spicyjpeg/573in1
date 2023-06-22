
#include "ps1/gpucmd.h"
#include "defs.hpp"
#include "gpu.hpp"
#include "uibase.hpp"
#include "uicommon.hpp"

namespace ui {

/* Common higher-level screens */

void PlaceholderScreen::draw(Context &ctx, bool active) const {
	_newLayer(ctx, 0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height);
	ctx.gpuCtx.drawRect(
		0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height, COLOR_WINDOW2
	);
}

MessageScreen::MessageScreen(void)
: ModalScreen(MODAL_WIDTH, MODAL_HEIGHT_FULL), _numButtons(0), _locked(false) {}

void MessageScreen::show(Context &ctx, bool goBack) {
	ModalScreen::show(ctx, goBack);

	_activeButton = 0;
	_buttonAnim.setValue(_getButtonWidth());
}

void MessageScreen::draw(Context &ctx, bool active) const {
	ModalScreen::draw(ctx, active);

	if (!active || !_numButtons)
		return;

	int buttonX = _width / 8;
	int buttonY = TITLE_BAR_HEIGHT + _height - (BUTTON_HEIGHT + MODAL_PADDING);
	gpu::RectWH rect;

	rect.y = buttonY + BUTTON_PADDING;
	rect.w = _getButtonWidth();
	rect.h = BUTTON_HEIGHT - BUTTON_PADDING * 2;

	for (int i = 0; i < _numButtons; i++) {
		rect.x = buttonX +
			(rect.w - ctx.font.getStringWidth(_buttons[i])) / 2;

		if (_locked) {
			ctx.gpuCtx.drawRect(
				buttonX, buttonY, rect.w, BUTTON_HEIGHT, COLOR_SHADOW, true
			);

			ctx.font.draw(ctx.gpuCtx, _buttons[i], rect, COLOR_TEXT2);
		} else {
			if (i == _activeButton) {
				ctx.gpuCtx.drawRect(
					buttonX, buttonY, rect.w, BUTTON_HEIGHT, COLOR_HIGHLIGHT2
				);
				ctx.gpuCtx.drawRect(
					buttonX, buttonY, _buttonAnim.getValue(ctx.time), BUTTON_HEIGHT,
					COLOR_HIGHLIGHT1
				);
			} else {
				ctx.gpuCtx.drawRect(
					buttonX, buttonY, rect.w, BUTTON_HEIGHT, COLOR_WINDOW3
				);
			}

			ctx.font.draw(ctx.gpuCtx, _buttons[i], rect, COLOR_TITLE);
		}

		buttonX += rect.w + BUTTON_SPACING;
	}
}

void MessageScreen::update(Context &ctx) {
	if (_locked)
		return;

	if (ctx.buttons.pressed(ui::BTN_LEFT)) {
		if (_activeButton > 0) {
			_activeButton--;

			_buttonAnim.setValue(ctx.time, 0, _getButtonWidth(), SPEED_FASTEST);
			ctx.sounds[SOUND_MOVE].play();
		} else {
			ctx.sounds[SOUND_CLICK].play();
		}
	}
	if (ctx.buttons.pressed(ui::BTN_RIGHT)) {
		if (_activeButton < (_numButtons - 1)) {
			_activeButton++;

			_buttonAnim.setValue(ctx.time, 0, _getButtonWidth(), SPEED_FASTEST);
			ctx.sounds[SOUND_MOVE].play();
		} else {
			ctx.sounds[SOUND_CLICK].play();
		}
	}
}

ProgressScreen::ProgressScreen(void)
: ModalScreen(MODAL_WIDTH, MODAL_HEIGHT_REDUCED) {}

void ProgressScreen::show(Context &ctx, bool goBack) {
	ModalScreen::show(ctx, goBack);

	_progressBarAnim.setValue(0);
}

void ProgressScreen::draw(Context &ctx, bool active) const {
	ModalScreen::draw(ctx, active);

	if (!active)
		return;

	int fullBarWidth = _width - MODAL_PADDING * 2;

	int barX = (_width - fullBarWidth) / 2;
	int barY = TITLE_BAR_HEIGHT + _height -
		(PROGRESS_BAR_HEIGHT + MODAL_PADDING);

	_setBlendMode(ctx, GP0_BLEND_SEMITRANS, true);

	ctx.gpuCtx.drawRect(
		barX, barY, fullBarWidth, PROGRESS_BAR_HEIGHT, COLOR_WINDOW3
	);
	ctx.gpuCtx.drawGradientRectH(
		barX, barY, _progressBarAnim.getValue(ctx.time), PROGRESS_BAR_HEIGHT,
		COLOR_PROGRESS2, COLOR_PROGRESS1
	);
}

TextScreen::TextScreen(void)
: _title(nullptr), _body(nullptr), _prompt(nullptr) {}

void TextScreen::draw(Context &ctx, bool active) const {
	int screenWidth  = ctx.gpuCtx.width  - SCREEN_MARGIN_X * 2;
	int screenHeight = ctx.gpuCtx.height - SCREEN_MARGIN_Y * 2;

	_newLayer(
		ctx, SCREEN_MARGIN_X, SCREEN_MARGIN_Y, screenWidth, screenHeight
	);

	gpu::Rect rect;

	rect.x1 = 0;
	rect.y1 = 0;
	rect.x2 = screenWidth;
	rect.y2 = gpu::FONT_LINE_HEIGHT;
	ctx.font.draw(ctx.gpuCtx, _title, rect, COLOR_TITLE);

	rect.y1 = gpu::FONT_LINE_HEIGHT + SCREEN_BLOCK_MARGIN;
	rect.y2 = screenHeight - SCREEN_PROMPT_HEIGHT;
	ctx.font.draw(ctx.gpuCtx, _body, rect, COLOR_TEXT1, true);

	rect.y1 = screenHeight - SCREEN_PROMPT_HEIGHT;
	rect.y2 = screenHeight;
	ctx.font.draw(ctx.gpuCtx, _prompt, rect, COLOR_TEXT1, true);
}

ImageScreen::ImageScreen(void)
: _imageScale(1), _imagePadding(0), _title(nullptr), _prompt(nullptr) {}

void ImageScreen::draw(Context &ctx, bool active) const {
	_newLayer(ctx, 0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height);

	if (_image.width && _image.height) {
		int x      = ctx.gpuCtx.width  / 2;
		int y      = ctx.gpuCtx.height / 2;
		int width  = _image.width  * _imageScale / 2;
		int height = _image.height * _imageScale / 2;

		if (_prompt)
			y -= (SCREEN_PROMPT_HEIGHT - gpu::FONT_LINE_HEIGHT) / 2;

		// Backdrop
		if (_imagePadding) {
			int _width  = width  + _imagePadding;
			int _height = height + _imagePadding;

			ctx.gpuCtx.drawRect(
				x - _width, y - _height, _width * 2, _height * 2, _backdropColor
			);
		}

		// Image
		_image.drawScaled(
			ctx.gpuCtx, x - width - 1, y - height - 1, width * 2, height * 2
		);
	}

	// Text
	gpu::Rect rect;

	rect.x1 = SCREEN_MARGIN_X;
	rect.y1 = SCREEN_MARGIN_Y;
	rect.x2 = ctx.gpuCtx.width - SCREEN_MARGIN_X;
	rect.y2 = SCREEN_MARGIN_Y + gpu::FONT_LINE_HEIGHT;
	ctx.font.draw(ctx.gpuCtx, _title, rect, COLOR_TITLE);

	rect.y1 = ctx.gpuCtx.height - (SCREEN_MARGIN_Y + SCREEN_PROMPT_HEIGHT);
	rect.y2 = ctx.gpuCtx.height - SCREEN_MARGIN_Y;
	ctx.font.draw(ctx.gpuCtx, _prompt, rect, COLOR_TEXT1, true);
}

ListScreen::ListScreen(void)
: _listLength(0), _prompt(nullptr) {}

void ListScreen::_drawItems(Context &ctx) const {
	int itemY      = _scrollAnim.getValue(ctx.time);
	int itemWidth  = _getItemWidth(ctx);
	int listHeight = _getListHeight(ctx);

	gpu::Rect rect;

	rect.x1 = LIST_BOX_PADDING + LIST_ITEM_PADDING;
	rect.x2 = itemWidth - LIST_ITEM_PADDING;
	rect.y2 = listHeight;

	for (int i = 0; (i < _listLength) && (itemY < listHeight); i++) {
		int itemHeight = gpu::FONT_LINE_HEIGHT + LIST_ITEM_PADDING * 2;
		if (i == _activeItem)
			itemHeight += gpu::FONT_LINE_HEIGHT;

		if ((itemY + itemHeight) >= 0) {
			if (i == _activeItem) {
				ctx.gpuCtx.drawRect(
					LIST_BOX_PADDING, itemY, itemWidth, itemHeight,
					COLOR_HIGHLIGHT2
				);
				ctx.gpuCtx.drawRect(
					LIST_BOX_PADDING, itemY, _itemAnim.getValue(ctx.time),
					itemHeight, COLOR_HIGHLIGHT1
				);

				rect.y1 = itemY + LIST_ITEM_PADDING + gpu::FONT_LINE_HEIGHT;
				ctx.font.draw(ctx.gpuCtx, _itemPrompt, rect, COLOR_SUBTITLE);
			}

			rect.y1 = itemY + LIST_ITEM_PADDING;
			ctx.font.draw(ctx.gpuCtx, _getItemName(ctx, i), rect, COLOR_TITLE);
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

	_newLayer(
		ctx, SCREEN_MARGIN_X, SCREEN_MARGIN_Y, screenWidth, screenHeight
	);

	// Text
	gpu::Rect rect;

	rect.x1 = 0;
	rect.y1 = 0;
	rect.x2 = screenWidth;
	rect.y2 = gpu::FONT_LINE_HEIGHT;
	ctx.font.draw(ctx.gpuCtx, _title, rect, COLOR_TITLE);

	rect.y1 = screenHeight - SCREEN_PROMPT_HEIGHT;
	rect.y2 = screenHeight;
	ctx.font.draw(ctx.gpuCtx, _prompt, rect, COLOR_TEXT1, true);

	_newLayer(
		ctx, SCREEN_MARGIN_X,
		SCREEN_MARGIN_Y + gpu::FONT_LINE_HEIGHT + SCREEN_BLOCK_MARGIN,
		screenWidth, listHeight
	);
	_setBlendMode(ctx, GP0_BLEND_SEMITRANS, true);

	// List box
	ctx.gpuCtx.drawRect(0, 0, screenWidth / 2, listHeight, COLOR_BOX1);
	ctx.gpuCtx.drawGradientRectH(
		screenWidth / 2, 0, screenWidth / 2, listHeight, COLOR_BOX1, COLOR_BOX2
	);

	if (_listLength) {
		_drawItems(ctx);

		// Up/down arrow icons
		gpu::RectWH iconRect;

		iconRect.x = screenWidth - (gpu::FONT_LINE_HEIGHT + LIST_BOX_PADDING);
		iconRect.w = gpu::FONT_LINE_HEIGHT;
		iconRect.h = gpu::FONT_LINE_HEIGHT;

		if (_activeItem) {
			iconRect.y = LIST_BOX_PADDING;
			ctx.font.draw(ctx.gpuCtx, CH_UP_ARROW, iconRect, COLOR_TEXT1);
		}
		if (_activeItem < (_listLength - 1)) {
			iconRect.y = listHeight - (gpu::FONT_LINE_HEIGHT + LIST_BOX_PADDING);
			ctx.font.draw(ctx.gpuCtx, CH_DOWN_ARROW, iconRect, COLOR_TEXT1);
		}
	}
}

void ListScreen::update(Context &ctx) {
	if (
		ctx.buttons.pressed(ui::BTN_LEFT) ||
		(ctx.buttons.repeating(ui::BTN_LEFT) && (_activeItem > 0))
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
		(ctx.buttons.repeating(ui::BTN_RIGHT) && (_activeItem < (_listLength - 1)))
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
	int itemHeight       = gpu::FONT_LINE_HEIGHT + LIST_ITEM_PADDING * 2;
	int activeItemHeight = itemHeight + gpu::FONT_LINE_HEIGHT;

	int topOffset     = _activeItem * itemHeight;
	int bottomOffset  = topOffset + activeItemHeight - _getListHeight(ctx);
	int currentOffset = -_scrollAnim.getTargetValue();

	if (topOffset < currentOffset)
		_scrollAnim.setValue(ctx.time, LIST_BOX_PADDING - topOffset, SPEED_FAST);
	else if (bottomOffset > currentOffset)
		_scrollAnim.setValue(ctx.time, -(LIST_BOX_PADDING + bottomOffset), SPEED_FAST);
}

HexEntryScreen::HexEntryScreen(void)
: _numDigits(0), _title(nullptr), _prompt(nullptr) {}

void HexEntryScreen::show(Context &ctx, bool goBack) {
	AnimatedScreen::show(ctx, goBack);

	_activeItem = 0;
}

void HexEntryScreen::draw(Context &ctx, bool active) const {
	int screenWidth  = ctx.gpuCtx.width  - SCREEN_MARGIN_X * 2;
	int screenHeight = ctx.gpuCtx.height - SCREEN_MARGIN_Y * 2;

	_newLayer(
		ctx, SCREEN_MARGIN_X, SCREEN_MARGIN_Y, screenWidth, screenHeight
	);

	/*gpu::RectWH rect;

	rect.x = 0;
	rect.y = 0;
	rect.w = screenWidth;
	rect.h = gpu::FONT_LINE_HEIGHT;
	ctx.font.draw(ctx.gpuCtx, _title, rect, COLOR_TITLE);

	rect.y = screenHeight - SCREEN_PROMPT_HEIGHT;
	rect.h = SCREEN_PROMPT_HEIGHT;
	ctx.font.draw(ctx.gpuCtx, _prompt, rect, COLOR_TEXT1, true);*/
}

void HexEntryScreen::update(Context &ctx) {
	if (ctx.buttons.held(ui::BTN_START)) {
	} else {
		if (
			ctx.buttons.pressed(ui::BTN_LEFT) ||
			(ctx.buttons.repeating(ui::BTN_LEFT) && (_activeItem > 0))
		) {
			if (_activeItem > 0) {
				_activeItem--;

				//_buttonAnim.setValue(ctx.time, 0, _getButtonWidth(), SPEED_FASTEST);
				ctx.sounds[SOUND_MOVE].play();
			} else {
				ctx.sounds[SOUND_CLICK].play();
			}
		}
		if (
			ctx.buttons.pressed(ui::BTN_RIGHT) ||
			(ctx.buttons.repeating(ui::BTN_RIGHT) && (_activeItem < (_numDigits - 1)))
		) {
			if (_activeItem < (_numDigits + _numButtons - 1)) {
				_activeItem++;

				//_buttonAnim.setValue(ctx.time, 0, _getButtonWidth(), SPEED_FASTEST);
				ctx.sounds[SOUND_MOVE].play();
			} else {
				ctx.sounds[SOUND_CLICK].play();
			}
		}
	}
}

}
