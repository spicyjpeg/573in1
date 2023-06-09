
#pragma once

/* FatFs configuration */

#define FF_FS_READONLY  0
#define FF_FS_MINIMIZE  0
#define FF_USE_FIND     0
#define FF_USE_MKFS     0
#define FF_USE_FASTSEEK 0
#define FF_USE_EXPAND   0
#define FF_USE_CHMOD    0
#define FF_USE_LABEL    1
#define FF_USE_FORWARD  0
#define FF_USE_STRFUNC  0

#define FF_CODE_PAGE   437
#define FF_USE_LFN     2
#define FF_MAX_LFN     255
#define FF_LFN_UNICODE 0
#define FF_LFN_BUF     255
#define FF_SFN_BUF     12
#define FF_FS_RPATH    0

#define FF_VOLUMES         2
#define FF_STR_VOLUME_ID   0
#define FF_MULTI_PARTITION 0

#define FF_MIN_SS   512
#define FF_MAX_SS   4096
#define FF_LBA64    1
#define FF_MIN_GPT  0x10000000
#define FF_USE_TRIM 1

#define FF_FS_TINY     0
#define FF_FS_EXFAT    1
#define FF_FS_NORTC    0
#define FF_FS_NOFSINFO 0

#define FF_FS_LOCK      0
#define FF_FS_REENTRANT 0

/* miniz configuration */

#define MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME

/* printf configuration */

#define PRINTF_DISABLE_SUPPORT_FLOAT
