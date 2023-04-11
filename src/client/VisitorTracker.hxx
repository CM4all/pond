// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/log/Chrono.hxx"

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

	uint64_t last_id = RandomVisitorId();

public:
	[[gnu::pure]]
	const char *MakeVisitorId(const char *remote_host,
				  Net::Log::TimePoint timestamp) noexcept;

	void Reset() noexcept {
		m.clear();

		/* not resetting last_id here because we just continue
		   our random sequence in the next file */
	}

private:
	uint64_t NewVisitorId() noexcept {
		return ++last_id;
	}

	static uint64_t RandomVisitorId() noexcept;
};
