
#pragma once

#ifndef VERSION
#define VERSION "<unknown build>"
#endif

#ifdef NDEBUG
#define VERSION_STRING VERSION
#else
#define VERSION_STRING VERSION "-debug"
#endif

#define EXTERNAL_DATA_DIR "cartdata"

enum Character : char {
	CH_UP_ARROW        = '\x80',
	CH_DOWN_ARROW      = '\x81',
	CH_LEFT_ARROW      = '\x82',
	CH_RIGHT_ARROW     = '\x83',
	CH_UP_ARROW_ALT    = '\x84',
	CH_DOWN_ARROW_ALT  = '\x85',
	CH_LEFT_ARROW_ALT  = '\x86',
	CH_RIGHT_ARROW_ALT = '\x87',
	CH_LEFT_BUTTON     = '\x90',
	CH_RIGHT_BUTTON    = '\x91',
	CH_START_BUTTON    = '\x92',
	CH_CLOSED_LOCK     = '\x93',
	CH_OPEN_LOCK       = '\x94',
	CH_DIR_ICON        = '\x95',
	CH_PARENT_DIR_ICON = '\x96',
	CH_FILE_ICON       = '\x97',
	CH_CHIP_ICON       = '\x98',
	CH_CART_ICON       = '\x99'
};
