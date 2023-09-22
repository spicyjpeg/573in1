
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

#define CH_UP_ARROW     "\x80"
#define CH_DOWN_ARROW   "\x81"
#define CH_LEFT_ARROW   "\x82"
#define CH_RIGHT_ARROW  "\x83"
#define CH_LEFT_BUTTON  "\x84"
#define CH_RIGHT_BUTTON "\x85"
#define CH_START_BUTTON "\x86"
#define CH_CLOSED_LOCK  "\x87"
#define CH_OPEN_LOCK    "\x88"
#define CH_CHIP_ICON    "\x89"
#define CH_CART_ICON    "\x8a"
