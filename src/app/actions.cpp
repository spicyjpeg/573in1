
#include "app/actions.hpp"
#include "app/app.hpp"
#include "defs.hpp"
#include "uibase.hpp"
#include "util.hpp"

/* Unlocked cartridge screens */

struct Action {
public:
	util::Hash name, prompt;
	void (CartActionsScreen::*target)(ui::Context &ctx);
};

static constexpr int _NUM_IDENTIFIED_ACTIONS   = 6;
static constexpr int _NUM_UNIDENTIFIED_ACTIONS = 4;

static const Action _IDENTIFIED_ACTIONS[]{
	{
		.name   = "CartActionsScreen.qrDump.name"_h,
		.prompt = "CartActionsScreen.qrDump.prompt"_h,
		.target = &CartActionsScreen::qrDump
	}, {
		.name   = "CartActionsScreen.hexdump.name"_h,
		.prompt = "CartActionsScreen.hexdump.prompt"_h,
		.target = &CartActionsScreen::hexdump
	}, {
		.name   = "CartActionsScreen.resetSystemID.name"_h,
		.prompt = "CartActionsScreen.resetSystemID.prompt"_h,
		.target = &CartActionsScreen::resetSystemID
	}, {
		.name   = "CartActionsScreen.editSystemID.name"_h,
		.prompt = "CartActionsScreen.editSystemID.prompt"_h,
		.target = &CartActionsScreen::editSystemID
	}, {
		.name   = "CartActionsScreen.reflash.name"_h,
		.prompt = "CartActionsScreen.reflash.prompt"_h,
		.target = &CartActionsScreen::reflash
	}, {
		.name   = "CartActionsScreen.erase.name"_h,
		.prompt = "CartActionsScreen.erase.prompt"_h,
		.target = &CartActionsScreen::erase
	}
};

static const Action _UNIDENTIFIED_ACTIONS[]{
	{
		.name   = "CartActionsScreen.qrDump.name"_h,
		.prompt = "CartActionsScreen.qrDump.prompt"_h,
		.target = &CartActionsScreen::qrDump
	}, {
		.name   = "CartActionsScreen.hexdump.name"_h,
		.prompt = "CartActionsScreen.hexdump.prompt"_h,
		.target = &CartActionsScreen::hexdump
	}, {
		.name   = "CartActionsScreen.reflash.name"_h,
		.prompt = "CartActionsScreen.reflash.prompt"_h,
		.target = &CartActionsScreen::reflash
	}, {
		.name   = "CartActionsScreen.erase.name"_h,
		.prompt = "CartActionsScreen.erase.prompt"_h,
		.target = &CartActionsScreen::erase
	}
};

const char *CartActionsScreen::_getItemName(ui::Context &ctx, int index) const {
	return STRH(_IDENTIFIED_ACTIONS[index].name);
}

void CartActionsScreen::qrDump(ui::Context &ctx) {
	APP->_setupWorker(&App::_qrCodeWorker);
	ctx.show(APP->_workerStatusScreen, false, true);
}

void CartActionsScreen::hexdump(ui::Context &ctx) {
	//ctx.show(APP->_hexdumpScreen, false, true);
}

void CartActionsScreen::resetSystemID(ui::Context &ctx) {
}

void CartActionsScreen::editSystemID(ui::Context &ctx) {
}

void CartActionsScreen::reflash(ui::Context &ctx) {
}

void CartActionsScreen::erase(ui::Context &ctx) {
}

void CartActionsScreen::show(ui::Context &ctx, bool goBack)  {
	_title      = STR("CartActionsScreen.title");
	_itemPrompt = STR("CartActionsScreen.itemPrompt");

	if (APP->_identified) {
		_prompt     = STRH(_IDENTIFIED_ACTIONS[0].prompt);
		_listLength = _NUM_IDENTIFIED_ACTIONS;
	} else {
		_prompt     = STRH(_UNIDENTIFIED_ACTIONS[0].prompt);
		_listLength = _NUM_UNIDENTIFIED_ACTIONS;
	}

	ListScreen::show(ctx, goBack);
}

void CartActionsScreen::update(ui::Context &ctx) {
	auto &action = APP->_identified
		? _IDENTIFIED_ACTIONS[_activeItem]
		: _UNIDENTIFIED_ACTIONS[_activeItem];

	_prompt = STRH(action.prompt);

	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		(this->*action.target)(ctx);
	if (ctx.buttons.held(ui::BTN_LEFT) && ctx.buttons.held(ui::BTN_RIGHT))
		ctx.show(APP->_cartInfoScreen, true, true);
}

static constexpr int _QR_CODE_SCALE   = 2;
static constexpr int _QR_CODE_PADDING = 6;

void QRCodeScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("QRCodeScreen.title");
	_prompt = STR("QRCodeScreen.prompt");

	_imageScale    = _QR_CODE_SCALE;
	_imagePadding  = _QR_CODE_SCALE * _QR_CODE_PADDING;
	_backdropColor = 0xffffff;

	ImageScreen::show(ctx, goBack);
}

void QRCodeScreen::update(ui::Context &ctx) {
	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_cartInfoScreen, true, true);
}
