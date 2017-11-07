/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "util/Compiler.h"

#include <string>

namespace Net { namespace Log { struct Datagram; }}

struct Filter {
	std::string site;

	gcc_pure
	bool operator()(const Net::Log::Datagram &d) const noexcept;
};
