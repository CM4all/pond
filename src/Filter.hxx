/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "util/Compiler.h"

#include <string>

#include <stdint.h>

namespace Net { namespace Log { struct Datagram; }}

struct Filter {
	std::string site;

	uint64_t since = 0, until = 0;

	gcc_pure
	bool operator()(const Net::Log::Datagram &d) const noexcept;
};
