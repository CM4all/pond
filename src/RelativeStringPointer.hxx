// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "RelativeBytePointer.hxx"

/**
 * Stores a pointer to a (nullable) C string located at a certain
 * offset after this object.
 *
 * @param offset_type an unsigned integer big enough to hold the
 * maximum possible offset
 */
template<std::unsigned_integral offset_type>
class RelativeStringPointer {
	RelativeBytePointer<offset_type> ptr;

public:
	constexpr RelativeStringPointer(const char *src) noexcept
		:ptr(reinterpret_cast<const std::byte *>(src)) {}

	constexpr operator bool() const noexcept {
		return ptr;
	}

	constexpr operator const char *() const noexcept {
		return reinterpret_cast<const char *>(ptr.get());
	}
};
