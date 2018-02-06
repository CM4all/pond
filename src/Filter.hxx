/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/log/Protocol.hxx"
#include "util/Compiler.h"

#include <string>
#include <set>

#include <stdint.h>

namespace Net { namespace Log { struct Datagram; }}

struct Filter {
	std::set<std::string> sites;

	uint64_t since = 0, until = 0;

	Net::Log::Type type = Net::Log::Type::UNSPECIFIED;

	bool HasOneSite() const noexcept {
		return !sites.empty() &&
			std::next(sites.begin()) == sites.end();
	}

	gcc_pure
	bool operator()(const Net::Log::Datagram &d) const noexcept;
};
