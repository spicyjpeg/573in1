
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

#define CH_UP_ARROW        "\x80"
#define CH_DOWN_ARROW      "\x81"
#define CH_LEFT_ARROW      "\x82"
#define CH_RIGHT_ARROW     "\x83"
#define CH_LEFT_BUTTON     "\x90"
#define CH_RIGHT_BUTTON    "\x91"
#define CH_START_BUTTON    "\x92"
#define CH_CLOSED_LOCK     "\x93"
#define CH_OPEN_LOCK       "\x94"
#define CH_DIR_ICON        "\x95"
#define CH_PARENT_DIR_ICON "\x96"
#define CH_FILE_ICON       "\x97"
#define CH_EXE_FILE_ICON   "\x98"
#define CH_DUMP_FILE_ICON  "\x99"
#define CH_CHIP_ICON       "\x9a"
#define CH_CART_ICON       "\x9b"
