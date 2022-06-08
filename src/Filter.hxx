/*
 * Copyright 2017-2022 CM4all GmbH
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
#include "net/log/Protocol.hxx"

#include <string>
#include <set>

#include <stdint.h>

struct SmallDatagram;
namespace Net { namespace Log { struct Datagram; }}

struct Filter {
	std::set<std::string> sites;

	Net::Log::TimePoint since = Net::Log::TimePoint::min();
	Net::Log::TimePoint until = Net::Log::TimePoint::max();

	Net::Log::Type type = Net::Log::Type::UNSPECIFIED;

	bool HasOneSite() const noexcept {
		return !sites.empty() &&
			std::next(sites.begin()) == sites.end();
	}

	[[gnu::pure]]
	bool operator()(const SmallDatagram &d) const noexcept;

	[[gnu::pure]]
	bool operator()(const Net::Log::Datagram &d) const noexcept;
};
