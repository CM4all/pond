// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "SmallDatagram.hxx"
#include "util/IntrusiveList.hxx"

#include <cstddef>
#include <span>

class Record final {
public:
	using ListHook = IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK>;
	ListHook per_site_list_hook;

private:
	uint64_t id;

	const size_t raw_size;

	SmallDatagram parsed;

public:
	/**
	 * Throws Net::Log::ProtocolError on error.
	 */
	Record(uint64_t _id, std::span<const std::byte> _raw);

	Record(const Record &) = delete;
	Record &operator=(const Record &) = delete;

	uint64_t GetId() const noexcept {
		return id;
	}

	std::span<const std::byte> GetRaw() const noexcept {
		return {(const std::byte *)(this + 1), raw_size};
	}

	const auto &GetParsed() const noexcept {
		return parsed;
	}

	bool IsOlderThan(Net::Log::TimePoint t) const noexcept {
		return parsed.HasTimestamp() && parsed.timestamp < t;
	}

	/**
	 * Like IsOlderThan(), but also return true if the time stamp
	 * is not known.  Used by Database::DeleteOlderThan().
	 */
	bool IsOlderThanOrUnknown(Net::Log::TimePoint t) const noexcept {
		return !parsed.HasTimestamp() || parsed.timestamp < t;
	}
};
