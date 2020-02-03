/*
 * Copyright 2017-2020 CM4all GmbH
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

#include "Filter.hxx"
#include "SmallDatagram.hxx"
#include "net/log/Datagram.hxx"

gcc_pure
static bool
MatchFilter(const char *value, const std::set<std::string> &filter) noexcept
{
	return filter.empty() || (value != nullptr &&
				  filter.find(value) != filter.end());
}

static bool
MatchTimestamp(Net::Log::TimePoint timestamp,
	       Net::Log::TimePoint since, Net::Log::TimePoint until)
{
	return timestamp >= since && timestamp <= until;
}

bool
Filter::operator()(const SmallDatagram &d) const noexcept
{
	return MatchFilter(d.site, sites) &&
		(type == Net::Log::Type::UNSPECIFIED ||
		 type == d.type) &&
		((since == Net::Log::TimePoint::min() &&
		  until == Net::Log::TimePoint::max()) ||
		 (d.HasTimestamp() &&
		  MatchTimestamp(d.timestamp, since, until)));
}

bool
Filter::operator()(const Net::Log::Datagram &d) const noexcept
{
	return MatchFilter(d.site, sites) &&
		(type == Net::Log::Type::UNSPECIFIED ||
		 type == d.type) &&
		((since == Net::Log::TimePoint::min() &&
		  until == Net::Log::TimePoint::max()) ||
		 (d.HasTimestamp() &&
		  MatchTimestamp(d.timestamp, since, until)));
}
