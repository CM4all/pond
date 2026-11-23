// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "RelativeStringPointer.hxx"
#include "net/log/Datagram.hxx"

/**
 * A smaller version of #Net::Log::Datagram with only the attributes
 * used by the Pond server.
 */
struct SmallDatagram {
	Net::Log::TimePoint timestamp;

	RelativeStringPointer<uint_least16_t> site;

	Net::Log::Type type = Net::Log::Type::UNSPECIFIED;

	constexpr SmallDatagram(const Net::Log::Datagram &src) noexcept
		:timestamp(src.timestamp), site(src.site),
		 type(src.type) {}

	constexpr bool HasTimestamp() const noexcept {
		return timestamp != Net::Log::TimePoint();
	}
};
