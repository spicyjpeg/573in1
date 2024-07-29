/*
 * 573in1 - Copyright (C) 2022-2024 spicyjpeg
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

namespace util {

static constexpr int TWEEN_UNIT = 1 << 12;

class LinearEasing {
public:
	template<typename T> static inline T apply(T value) {
		return value;
	}
};

class QuadInEasing {
public:
	template<typename T> static inline T apply(T value) {
		return (value * value) / TWEEN_UNIT;
	}
};

class QuadOutEasing {
public:
	template<typename T> static inline T apply(T value) {
		return (value * 2) - ((value * value) / TWEEN_UNIT);
	}
};

template<typename T, typename E> class Tween {
private:
	T   _base, _delta;
	int _endTime, _timeScale;

public:
	inline Tween(void) {
		setValue(static_cast<T>(0));
	}
	inline Tween(T start) {
		setValue(start);
	}

	inline T getTargetValue(void) const {
		return _base + _delta;
	}
	inline bool isDone(int time) const {
		return time >= _endTime;
	}
	inline void setValue(int time, T target, int duration) {
		setValue(time, getValue(time), target, duration);
	}

	void setValue(int time, T start, T target, int duration);
	void setValue(T target);
	T getValue(int time) const;
};

}
