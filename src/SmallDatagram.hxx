// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/log/Datagram.hxx"

/**
 * A smaller version of #Net::Log::Datagram with only the attributes
 * used by the Pond server.
 */
struct SmallDatagram {
	Net::Log::TimePoint timestamp;

	const char *site;

	Net::Log::Type type = Net::Log::Type::UNSPECIFIED;

	SmallDatagram() = default;

	constexpr SmallDatagram(const Net::Log::Datagram &src) noexcept
		:timestamp(src.timestamp), site(src.site),
		 type(src.type) {}

	constexpr bool HasTimestamp() const noexcept {
		return timestamp != Net::Log::TimePoint();
	}
};
