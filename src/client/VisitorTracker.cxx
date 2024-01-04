// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "VisitorTracker.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "system/Urandom.hxx"

#include <fmt/core.h>

#include <chrono>

const char *
VisitorTracker::MakeVisitorId(const char *remote_host,
			      Net::Log::TimePoint timestamp) noexcept
{
	auto i = m.emplace(remote_host, Visitor{});

	if (i.second || !i.first->second.CheckTimestamp(timestamp)) {
		const uint64_t id = NewVisitorId();
		i.first->second.id = FmtBuffer<64>("{:x}", id);
	}

	i.first->second.last_seen = timestamp;

	return i.first->second.id.c_str();
}

uint64_t
VisitorTracker::RandomVisitorId() noexcept
{
	uint64_t result;

	try {
		UrandomFill(std::as_writable_bytes(std::span{&result, 1}));
	} catch (...) {
		/* getrandom() didn't work: fall back to the
		   high-resolution clock, which is good enough */
		result ^= std::chrono::high_resolution_clock::now().time_since_epoch().count();
	}

	return result;
}
