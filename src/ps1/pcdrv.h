/*
 * ps1-bare-metal - (C) 2023 spicyjpeg
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <stddef.h>

typedef enum {
	PCDRV_MODE_READ       = 0,
	PCDRV_MODE_WRITE      = 1,
	PCDRV_MODE_READ_WRITE = 2
} PCDRVOpenMode;

typedef enum {
	PCDRV_SEEK_SET = 0,
	PCDRV_SEEK_CUR = 1,
	PCDRV_SEEK_END = 2
} PCDRVSeekMode;

#ifdef __cplusplus
extern "C" {
#endif

int pcdrvInit(void);
int pcdrvCreate(const char *path);
int pcdrvOpen(const char *path, PCDRVOpenMode mode);
int pcdrvClose(int fd);
int pcdrvRead(int fd, void *data, size_t length);
int pcdrvWrite(int fd, const void *data, size_t length);
int pcdrvSeek(int fd, int offset, PCDRVSeekMode mode);

#ifdef __cplusplus
}
#endif
