
#include "app/actions.hpp"
#include "app/app.hpp"
#include "defs.hpp"
#include "uibase.hpp"
#include "util.hpp"

/* Unlocked cartridge screens */

struct Action {
public:
	util::Hash name, prompt;
	void (App::*target)(void);
};

// TODO: implement these
static const Action _IDENTIFIED_ACTIONS[]{
	{
		.name   = "CartActionsScreen.qrDump.name"_h,
		.prompt = "CartActionsScreen.qrDump.prompt"_h,
		.target = nullptr
	}, {
		.name   = "CartActionsScreen.hexdump.name"_h,
		.prompt = "CartActionsScreen.hexdump.prompt"_h,
		.target = nullptr
	}, {
		.name   = "CartActionsScreen.resetSystemID.name"_h,
		.prompt = "CartActionsScreen.resetSystemID.prompt"_h,
		.target = nullptr
	}, {
		.name   = "CartActionsScreen.editSystemID.name"_h,
		.prompt = "CartActionsScreen.editSystemID.prompt"_h,
		.target = nullptr
	}, {
		.name   = "CartActionsScreen.reflash.name"_h,
		.prompt = "CartActionsScreen.reflash.prompt"_h,
		.target = nullptr
	}, {
		.name   = "CartActionsScreen.erase.name"_h,
		.prompt = "CartActionsScreen.erase.prompt"_h,
		.target = nullptr
	}
};

static const Action _UNIDENTIFIED_ACTIONS[]{
	{
		.name   = "CartActionsScreen.qrDump.name"_h,
		.prompt = "CartActionsScreen.qrDump.prompt"_h,
		.target = nullptr
	}, {
		.name   = "CartActionsScreen.hexdump.name"_h,
		.prompt = "CartActionsScreen.hexdump.prompt"_h,
		.target = nullptr
	}, {
		.name   = "CartActionsScreen.reflash.name"_h,
		.prompt = "CartActionsScreen.reflash.prompt"_h,
		.target = nullptr
	}, {
		.name   = "CartActionsScreen.erase.name"_h,
		.prompt = "CartActionsScreen.erase.prompt"_h,
		.target = nullptr
	}
};

const char *CartActionsScreen::_getItemName(ui::Context &ctx, int index) const {
	return STRH(_IDENTIFIED_ACTIONS[index].name);
}

void CartActionsScreen::show(ui::Context &ctx, bool goBack)  {
	_title      = STR("CartActionsScreen.title");
	_prompt     = STRH(_IDENTIFIED_ACTIONS[0].prompt);
	_itemPrompt = STR("CartActionsScreen.itemPrompt");

	_listLength = 6;

	ListScreen::show(ctx, goBack);
}

void CartActionsScreen::update(ui::Context &ctx) {
	_prompt = STRH(_IDENTIFIED_ACTIONS[_activeItem].prompt);

	ListScreen::update(ctx);

	// TODO: implement this
	if (ctx.buttons.pressed(ui::BTN_START)) {
		//APP->_setupWorker(&App::_qrCodeWorker);
		//ctx.show(APP->_workerStatusScreen, false, true);
	}
	if (ctx.buttons.held(ui::BTN_LEFT) && ctx.buttons.held(ui::BTN_RIGHT)) {
		ctx.show(APP->_cartInfoScreen, true, true);
	}
}

static constexpr int QR_CODE_SCALE = 2;

void QRCodeScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("QRCodeScreen.title");
	_prompt = STR("QRCodeScreen.prompt");

	_imageScale    = QR_CODE_SCALE;
	_imagePadding  = QR_CODE_SCALE * 6;
	_backdropColor = 0xffffff;

	ImageScreen::show(ctx, goBack);
}

void QRCodeScreen::update(ui::Context &ctx) {
	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_cartInfoScreen, true, true);
}
