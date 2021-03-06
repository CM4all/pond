/*
 * Copyright 2017-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "VisitorTracker.hxx"
#include "system/Urandom.hxx"

#include <chrono>

#include <inttypes.h>

const char *
VisitorTracker::MakeVisitorId(const char *remote_host,
			      Net::Log::TimePoint timestamp) noexcept
{
	auto i = m.emplace(remote_host, Visitor{});

	if (i.second || !i.first->second.CheckTimestamp(timestamp)) {
		const uint64_t id = NewVisitorId();
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%" PRIx64, id);
		i.first->second.id = buffer;
	}

	i.first->second.last_seen = timestamp;

	return i.first->second.id.c_str();
}

uint64_t
VisitorTracker::RandomVisitorId() noexcept
{
	uint64_t result;

	try {
		UrandomFill(&result, sizeof(result));
	} catch (...) {
		/* getrandom() didn't work: fall back to the
		   high-resolution clock, which is good enough */
		result ^= std::chrono::high_resolution_clock::now().time_since_epoch().count();
	}

	return result;
}
