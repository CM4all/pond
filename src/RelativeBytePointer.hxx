// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cassert>
#include <cstddef> // for std::byte
#include <concepts> // for std::unsigned_integral
#include <limits>

/**
 * Stores a (nullable) pointer to an object located at a certain
 * offset after this object.
 *
 * @param offset_type an unsigned integer big enough to hold the
 * maximum possible offset
 */
template<std::unsigned_integral offset_type>
class RelativeBytePointer {
	/**
	 * The offset of the object relative to the last byte of this
	 * object.  Zero encodes the null pointer.
	 */
	offset_type offset;

public:
	constexpr RelativeBytePointer(const std::byte *src) noexcept
		:offset(0)
	{
		if (src != nullptr) {
			assert(src >= AfterThis());
			assert(src < AfterThis() + std::numeric_limits<offset_type>::max());

			offset = src - AfterThis() + 1;
		}
	}

	/* copying isn't allowed because the copy wouldn't have the
	   adjacent string data */
	RelativeBytePointer(const RelativeBytePointer &) = delete;
	RelativeBytePointer &operator=(const RelativeBytePointer &) = delete;

	constexpr operator bool() const noexcept {
		return offset != 0;
	}

	constexpr const std::byte *require() const noexcept {
		assert(*this);

		return AfterThis() + offset - 1;
	}

	constexpr const std::byte *get() const noexcept {
		return *this
			? require()
			: nullptr;
	}

	constexpr operator const std::byte *() const noexcept {
		return get();
	}

private:
	constexpr const std::byte *AfterThis() const noexcept {
		return reinterpret_cast<const std::byte *>(this + 1);
	}
};
