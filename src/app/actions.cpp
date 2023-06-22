
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

static constexpr int _NUM_IDENTIFIED_ACTIONS   = 7;
static constexpr int _NUM_UNIDENTIFIED_ACTIONS = 5;

static const Action _ACTIONS[]{
	{
		.name   = "CartActionsScreen.qrDump.name"_h,
		.prompt = "CartActionsScreen.qrDump.prompt"_h,
		.target = &CartActionsScreen::qrDump
	}, {
		.name   = "CartActionsScreen.hddDump.name"_h,
		.prompt = "CartActionsScreen.hddDump.prompt"_h,
		.target = &CartActionsScreen::hddDump
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
	}, {
		.name   = "CartActionsScreen.resetSystemID.name"_h,
		.prompt = "CartActionsScreen.resetSystemID.prompt"_h,
		.target = &CartActionsScreen::resetSystemID
	}, {
		.name   = "CartActionsScreen.editSystemID.name"_h,
		.prompt = "CartActionsScreen.editSystemID.prompt"_h,
		.target = &CartActionsScreen::editSystemID
	}
};

const char *CartActionsScreen::_getItemName(ui::Context &ctx, int index) const {
	return STRH(_ACTIONS[index].name);
}

void CartActionsScreen::qrDump(ui::Context &ctx) {
	APP->_setupWorker(&App::_qrCodeWorker);
	ctx.show(APP->_workerStatusScreen, false, true);
}

void CartActionsScreen::hddDump(ui::Context &ctx) {
	APP->_errorScreen.setMessage(*this, STR("CartActionsScreen.hddDump.error"));

	APP->_setupWorker(&App::_hddDumpWorker);
	ctx.show(APP->_workerStatusScreen, false, true);
}

void CartActionsScreen::hexdump(ui::Context &ctx) {
}

void CartActionsScreen::reflash(ui::Context &ctx) {
}

void CartActionsScreen::erase(ui::Context &ctx) {
	APP->_confirmScreen.setMessage(
		*this,
		[](ui::Context &ctx) {
			//APP->_setupWorker(&App::_eraseWorker);
			//ctx.show(APP->_workerStatusScreen, false, true);
		},
		STR("CartActionsScreen.erase.confirm")
	);

	ctx.show(APP->_confirmScreen, false, true);
}

void CartActionsScreen::resetSystemID(ui::Context &ctx) {
}

void CartActionsScreen::editSystemID(ui::Context &ctx) {
}

void CartActionsScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("CartActionsScreen.title");
	_prompt     = STRH(_ACTIONS[0].prompt);
	_itemPrompt = STR("CartActionsScreen.itemPrompt");

#if 0 // TODO
	_listLength = APP->_identified
		? _NUM_IDENTIFIED_ACTIONS
		: _NUM_UNIDENTIFIED_ACTIONS;
#else
	_listLength = 2;
#endif

	ListScreen::show(ctx, goBack);
}

void CartActionsScreen::update(ui::Context &ctx) {
	auto &action = _ACTIONS[_activeItem];
	_prompt      = STRH(action.prompt);

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
