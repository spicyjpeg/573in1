/*
 * 573in1 - Copyright (C) 2022-2025 spicyjpeg
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

#define CH_UP_ARROW        "\u25b4"
#define CH_DOWN_ARROW      "\u25be"
#define CH_LEFT_ARROW      "\u25c2"
#define CH_RIGHT_ARROW     "\u25b8"
#define CH_UP_ARROW_ALT    "\u2191"
#define CH_DOWN_ARROW_ALT  "\u2193"
#define CH_LEFT_ARROW_ALT  "\u2190"
#define CH_RIGHT_ARROW_ALT "\u2192"
#define CH_INVALID_CHAR    "\ufffd"

#define CH_PS1_CIRCLE_BUTTON   "\u25cb"
#define CH_PS1_X_BUTTON        "\u2715"
#define CH_PS1_TRIANGLE_BUTTON "\u25b3"
#define CH_PS1_SQUARE_BUTTON   "\u25a1"
#define CH_PS1_SELECT_BUTTON   "\u25ac"
#define CH_PS1_START_BUTTON    "\u25ba"

#define CH_LEFT_BUTTON  "\u25c1"
#define CH_RIGHT_BUTTON "\u25b7"
#define CH_START_BUTTON "\u25ad"

#define CH_CLOSED_LOCK "\U0001f512"
#define CH_OPEN_LOCK   "\U0001f513"

#define CH_CDROM_ICON         "\U0001f4bf"
#define CH_HDD_ICON           "\U0001f4bd"
#define CH_MEMORY_CARD_ICON   "\U0001f4be"
#define CH_POCKETSTATION_ICON "\U0001f4f1"
#define CH_HOST_ICON          "\U0001f50c"
#define CH_DIR_ICON           "\U0001f4c1"
#define CH_PARENT_DIR_ICON    "\U0001f4c2"
#define CH_FILE_ICON          "\U0001f4c4"
