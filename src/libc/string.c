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

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Character manipulation */

int isprint(int ch) {
	return (ch >= ' ') && (ch <= '~');
}

int isgraph(int ch) {
	return (ch > ' ') && (ch <= '~');
}

int isspace(int ch) {
	return (ch == ' ') || ((ch >= '\t') && (ch <= '\r'));
}

int isblank(int ch) {
	return (ch == ' ') || (ch == '\t');
}

int isalpha(int ch) {
	return ((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z'));
}

int isdigit(int ch) {
	return (ch >= '0') && (ch <= '9');
}

int tolower(int ch) {
	if ((ch >= 'A') && (ch <= 'Z'))
		ch += 'a' - 'A';

	return ch;
}

int toupper(int ch) {
	if ((ch >= 'a') && (ch <= 'z'))
		ch += 'A' - 'a';

	return ch;
}

/* Memory buffer manipulation */

#if 0
void *memset(void *dest, int ch, size_t count) {
	uint8_t *_dest = (uint8_t *) dest;

	for (; count; count--)
		*(_dest++) = (uint8_t) ch;

	return dest;
}
#endif

void *memcpy(void *restrict dest, const void *restrict src, size_t count) {
	uint8_t       *_dest = (uint8_t *) dest;
	const uint8_t *_src  = (const uint8_t *) src;

	for (; count; count--)
		*(_dest++) = *(_src++);

	return dest;
}

void *memccpy(void *restrict dest, const void *restrict src, int ch, size_t count) {
	uint8_t       *_dest = (uint8_t *) dest;
	const uint8_t *_src  = (const uint8_t *) src;

	for (; count; count--) {
		uint8_t a = *(_src++);

		*(_dest++) = a;
		if (a == ch)
			return (void *) _dest;
	}

	return 0;
}

void *memmove(void *dest, const void *src, size_t count) {
	uint8_t       *_dest = (uint8_t *) dest;
	const uint8_t *_src  = (const uint8_t *) src;

	if (_dest == _src)
		return dest;
	if ((_dest >= &_src[count]) || (&_dest[count] <= _src))
		return memcpy(dest, src, count);

	if (_dest < _src) { // Copy forwards
		for (; count; count--)
			*(_dest++) = *(_src++);
	} else { // Copy backwards
		_src  += count;
		_dest += count;

		for (; count; count--)
			*(--_dest) = *(--_src);
	}

	return dest;
}

int memcmp(const void *lhs, const void *rhs, size_t count) {
	const uint8_t *_lhs = (const uint8_t *) lhs;
	const uint8_t *_rhs = (const uint8_t *) rhs;

	for (; count; count--) {
		uint8_t a = *(_lhs++), b = *(_rhs++);

		if (a != b)
			return a - b;
	}

	return 0;
}

void *memchr(const void *ptr, int ch, size_t count) {
	const uint8_t *_ptr = (const uint8_t *) ptr;

	for (; count; count--, _ptr++) {
		if (*_ptr == ch)
			return (void *) _ptr;
	}

	return 0;
}

/* String manipulation */

char *strcpy(char *restrict dest, const char *restrict src) {
	char *_dest = dest;

	while (*src)
		*(_dest++) = *(src++);

	*_dest = 0;
	return dest;
}

char *strncpy(char *restrict dest, const char *restrict src, size_t count) {
	char *_dest = dest;

	for (; count && *src; count--)
		*(_dest++) = *(src++);
	for (; count; count--)
		*(_dest++) = 0;

	return dest;
}

int strcmp(const char *lhs, const char *rhs) {
	for (;;) {
		char a = *(lhs++), b = *(rhs++);

		if (a != b)
			return a - b;
		if (!a && !b)
			return 0;
	}
}

int strncmp(const char *lhs, const char *rhs, size_t count) {
	for (; count && *lhs && *rhs; count--) {
		char a = *(lhs++), b = *(rhs++);

		if (a != b)
			return a - b;
	}

	return 0;
}

char *strchr(const char *str, int ch) {
	for (; *str; str++) {
		if (*str == ch)
			return (char *) str;
	}

	return 0;
}

char *strrchr(const char *str, int ch) {
	size_t length = strlen(str);

	for (str += length; length; length--) {
		str--;
		if (*str == ch)
			return (char *) str;
	}

	return 0;
}

char *strpbrk(const char *str, const char *breakset) {
	for (; *str; str++) {
		char a = *str;

		for (const char *ch = breakset; *ch; ch++) {
			if (a == *ch)
				return (char *) str;
		}
	}

	return 0;
}

char *strstr(const char *str, const char *substr) {
	size_t length = strlen(substr);

	if (!length)
		return (char *) str;

	for (; *str; str++) {
		if (!memcmp(str, substr, length))
			return (char *) str;
	}

	return 0;
}

size_t strlen(const char *str) {
	size_t length = 0;

	for (; *str; str++)
		length++;

	return length;
}

// Non-standard, used internally
size_t strnlen(const char *str, size_t count) {
	size_t length = 0;

	for (; *str && (length < count); str++)
		length++;

	return length;
}

char *strcat(char *restrict dest, const char *restrict src) {
	char *_dest = &dest[strlen(dest)];

	while (*src)
		*(_dest++) = *(src++);

	*_dest = 0;
	return dest;
}

char *strncat(char *restrict dest, const char *restrict src, size_t count) {
	char *_dest = &dest[strlen(dest)];

	for (; count && *src; count--)
		*(_dest++) = *(src++);

	*_dest = 0;
	return dest;
}

char *strdup(const char *str) {
	size_t length = strlen(str) + 1;
	char   *copy  = malloc(length);

	if (!copy)
		return 0;

	memcpy(copy, str, length);
	return copy;
}

char *strndup(const char *str, size_t count) {
	size_t length = strnlen(str, count) + 1;
	char   *copy  = malloc(length);

	if (!copy)
		return 0;

	memcpy(copy, str, length);
	return copy;
}

/* String tokenizer */

static char *_strtokPtr = 0, *_strtokEndPtr = 0;

char *strtok(char *restrict str, const char *restrict delim) {
	if (str) {
		_strtokPtr    = str;
		_strtokEndPtr = &str[strlen(str)];
	}

	if (_strtokPtr >= _strtokEndPtr)
		return 0;
	if (!(*_strtokPtr))
		return 0;

	char *split = strstr(_strtokPtr, delim);
	char *token = _strtokPtr;

	if (split) {
		*(split++)  = 0;
		_strtokPtr = split;
	} else {
		_strtokPtr += strlen(token);
	}

	return token;
}

/* Number parsers */

long long strtoll(const char *restrict str, char **restrict str_end, int base) {
	if (!str)
		return 0;

	while (isspace(*str))
		str++;

	int negative = (*str == '-');
	if (negative)
		str++;

	while (isspace(*str))
		str++;

	// Parse any base prefix if present. If a base was specified make sure it
	// matches, otherwise use it to determine which base the value is in.
	long long value = 0;

	if (*str == '0') {
		int _base;

		switch (str[1]) {
			case 0:
				goto _exit;

			case 'X':
			case 'x':
				_base = 16;
				str  += 2;
				break;

			case 'O':
			case 'o':
				_base = 8;
				str  += 2;
				break;

			case 'B':
			case 'b':
				_base = 2;
				str  += 2;
				break;

			default:
				// Numbers starting with a zero are *not* interpreted as octal
				// unless base = 8.
				_base = 0;
				str++;
		}

		if (!base)
			base = _base;
		else if (base != _base)
			return 0;
	}

	if (!base)
		base = 10;
	else if ((base < 2) || (base > 36))
		return 0;

	// Parse the actual value.
	for (; *str; str++) {
		char ch = *str;
		int  digit;

		switch (ch) {
			case '0' ... '9':
				digit = ch - '0';
				break;

			case 'A' ... 'Z':
				digit = (ch - 'A') + 10;
				break;

			case 'a' ... 'z':
				digit = (ch - 'a') + 10;
				break;

			default:
				goto _exit;
		}

		value = (value * base) + digit;
	}

_exit:
	if (str_end)
		*str_end = (char *) str;

	return negative ? (-value) : value;
}

long strtol(const char *restrict str, char **restrict str_end, int base) {
	return (long) strtoll(str, str_end, base);
}
