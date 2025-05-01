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

#include <stddef.h>
#include <stdint.h>
#include "common/util/templates.hpp"

namespace util {

/* Simple ring buffer */

template<typename T, size_t N> class RingBuffer {
private:
	uint8_t _items[N][alignedSizeOf<T>()];
	size_t  _head, _tail;

	inline T *_get(size_t index) {
		return reinterpret_cast<T *>(_items[index]);
	}

public:
	size_t length;

	inline RingBuffer(void)
	:
		_head(0),
		_tail(0),
		length(0) {}
	inline ~RingBuffer(void) {
		// Ensure the destructor is called for each item left in the buffer.
		while (length > 0)
			popItem();
	}

	inline T *pushItem(void) {
		if (length >= N)
			return nullptr;

		auto item = new (_get(_tail)) T();

		length++;
		_tail++;
		_tail %= N;
		return item;
	}
	inline T *pushItem(T &&obj) {
		if (length >= N)
			return nullptr;

		auto item = new (_get(_tail)) T(static_cast<T &&>(obj));

		length++;
		_tail++;
		_tail %= N;
		return item;
	}
	inline bool popItem(void) {
		if (!length)
			return false;

		_get(_head)->~T();

		length--;
		_head++;
		_head %= N;
		return true;
	}

	inline T *getHead(void) const {
		return length ? _get(_head) : nullptr;
	}
	inline T *getTail(void) const {
		return length ? _get(_tail) : nullptr;
	}
};

/* Simple managed pointer */

class Data {
public:
	void   *ptr;
	size_t length;
	void   (*destructor)(void *ptr);

	inline ~Data(void) {
		destroy();
	}
	template<typename T> inline T *allocate(size_t count = 1) {
		return reinterpret_cast<T *>(allocate(sizeof(T) * count));
	}
	template<typename T> inline T *as(void) {
		return reinterpret_cast<T *>(ptr);
	}
	template<typename T> inline const T *as(void) const {
		return reinterpret_cast<const T *>(ptr);
	}

	Data(void);
	Data(Data &&source);
	void *allocate(size_t _length);
	void destroy(void);
};

/* Delegate class (callable/lambda invoker) */

static constexpr size_t MAX_DELEGATE_LENGTH = 16;

class Delegate {
private:
	uint8_t _obj[MAX_DELEGATE_LENGTH];
	void    (*_invoker)(void *ptr);
	void    (*_destructor)(void *ptr);

public:
	inline ~Delegate(void) {
		destroy();
	}
	template<typename T> inline void bind(T &&func) {
		static_assert(
			sizeof(T) <= MAX_DELEGATE_LENGTH,
			"callable object is too large for delegate"
		);
		static_assert(
			!(alignof(Delegate) % alignof(T)),
			"callable object is not guaranteed to be properly aligned"
		);

		if (_destructor)
			_destructor(_obj);

		new (_obj) T(static_cast<T &&>(func));

		_invoker    = [](void *ptr) {
			reinterpret_cast<T *>(ptr)();
		};
		_destructor = [](void *ptr) {
			reinterpret_cast<T *>(ptr)->~T();
		};
	}
	inline bool isBound(void) const {
		return bool(_invoker);
	}

	Delegate(void);
	void destroy(void);
	bool invoke(void);
};

}
