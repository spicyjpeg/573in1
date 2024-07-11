
#pragma once

#ifndef VERSION
#define VERSION "<unknown build>"
#endif

#ifndef EXTERNAL_DATA_DIR
#define EXTERNAL_DATA_DIR "hdd:/573in1"
#endif

#ifdef NDEBUG
#define VERSION_STRING VERSION
#else
#define VERSION_STRING VERSION "-debug"
#endif

enum Character : char {
	CH_UP_ARROW        = '\x80',
	CH_DOWN_ARROW      = '\x81',
	CH_LEFT_ARROW      = '\x82',
	CH_RIGHT_ARROW     = '\x83',
	CH_UP_ARROW_ALT    = '\x84',
	CH_DOWN_ARROW_ALT  = '\x85',
	CH_LEFT_ARROW_ALT  = '\x86',
	CH_RIGHT_ARROW_ALT = '\x87',

	CH_LEFT_BUTTON  = '\x90',
	CH_RIGHT_BUTTON = '\x91',
	CH_START_BUTTON = '\x92',
	CH_CLOSED_LOCK  = '\x93',
	CH_OPEN_LOCK    = '\x94',
	CH_CHIP_ICON    = '\x95',
	CH_CART_ICON    = '\x96',

	CH_CDROM_ICON      = '\xa0',
	CH_HDD_ICON        = '\xa1',
	CH_HOST_ICON       = '\xa2',
	CH_DIR_ICON        = '\xa3',
	CH_PARENT_DIR_ICON = '\xa4',
	CH_FILE_ICON       = '\xa5'
};
