// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "RelativeBytePointer.hxx"
#include "net/log/Chrono.hxx"
#include "util/UnalignedBigEndian.hxx"

/**
 * Stores a pointer to a (nullable) Pond time stamp a certain offset
 * after this object.
 *
 * @param offset_type an unsigned integer big enough to hold the
 * maximum possible offset
 */
template<std::unsigned_integral offset_type>
class RelativeTimePointer {
	RelativeBytePointer<offset_type> ptr;

	using Span = const std::span<const std::byte, 8>;

public:
	explicit constexpr RelativeTimePointer(Span src) noexcept
		:ptr(src.data()) {}

	constexpr operator bool() const noexcept {
		return ptr;
	}

	constexpr operator Net::Log::TimePoint() const noexcept {
		if (!ptr)
			return {};

		return Decode();
	}

	constexpr auto operator<=>(const Net::Log::TimePoint &other) const noexcept {
		return Decode() <=> other;
	}

	constexpr auto operator+(const Net::Log::Duration &d) const noexcept {
		return Decode() + d;
	}

private:
	constexpr auto Decode() const noexcept {
		const Span src{ptr.require(), 8};
		return Net::Log::TimePoint{Net::Log::Duration{ReadUnalignedBE64(src)}};
	}
};
