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

#include <assert.h>
#include <stdint.h>
#include "common/util/tween.hpp"

namespace util {

template<typename T, typename E> void Tween<T, E>::setValue(
	int time, T start, T target, int duration
) {
	//assert(duration <= 0x800);

	_base  = start;
	_delta = target - start;

	_endTime   = time + duration;
	_timeScale = TWEEN_UNIT / duration;
}

template<typename T, typename E> void Tween<T, E>::setValue(T target) {
	_base  = target;
	_delta = static_cast<T>(0);

	_endTime = 0;
}

template<typename T, typename E> T Tween<T, E>::getValue(int time) const {
	int remaining = time - _endTime;

	if (remaining >= 0)
		return _base + _delta;

	return _base + (
		_delta * E::apply(remaining * _timeScale + TWEEN_UNIT)
	) / TWEEN_UNIT;
}

template class Tween<int, LinearEasing>;
template class Tween<int, QuadInEasing>;
template class Tween<int, QuadOutEasing>;
template class Tween<uint16_t, LinearEasing>;
template class Tween<uint16_t, QuadInEasing>;
template class Tween<uint16_t, QuadOutEasing>;
template class Tween<uint32_t, LinearEasing>;
template class Tween<uint32_t, QuadInEasing>;
template class Tween<uint32_t, QuadOutEasing>;

}
