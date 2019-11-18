/*
 * Copyright 2017-2019 Content Management AG
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

#pragma once

#include "net/log/Chrono.hxx"
#include "util/Compiler.h"

#include <string>
#include <unordered_map>

#include <stdint.h>

class VisitorTracker {
	static constexpr Net::Log::Duration max_idle =
		std::chrono::minutes(30);

	struct Visitor {
		std::string id;

		Net::Log::TimePoint last_seen;

		bool CheckTimestamp(Net::Log::TimePoint new_timestamp) const noexcept {
			return new_timestamp < last_seen + max_idle;
		}
	};

	std::unordered_map<std::string, Visitor> m;

	uint64_t last_id = 0;

public:
	gcc_pure
	const char *MakeVisitorId(const char *remote_host,
				  Net::Log::TimePoint timestamp) noexcept;

	void Reset() noexcept {
		m.clear();
		last_id = 0;
	}

private:
	uint64_t NewVisitorId() noexcept {
		return ++last_id;
	}
};
