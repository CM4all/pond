// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Filter.hxx"
#include "SmallDatagram.hxx"
#include "net/log/Datagram.hxx"
#include "net/log/Parser.hxx"
#include "util/StringCompare.hxx"

static std::string_view
NullableStringView(const char *s) noexcept
{
	return s != nullptr ? std::string_view{s} : std::string_view{};
}

[[gnu::pure]]
static bool
MatchFilter(const char *value, const std::set<std::string, std::less<>> &filter) noexcept
{
	return filter.empty() || (value != nullptr && filter.contains(NullableStringView(value)));
}

static constexpr bool
MatchTimestamp(Net::Log::TimePoint timestamp,
	       Net::Log::TimePoint since, Net::Log::TimePoint until) noexcept
{
	return timestamp >= since && timestamp <= until;
}

[[gnu::pure]]
static bool
MatchHttpUriStartsWith(const char *http_uri,
		       std::string_view http_uri_starts_with) noexcept
{
	return http_uri_starts_with.empty() ||
		(http_uri != nullptr &&
		 StringStartsWith(http_uri, http_uri_starts_with));
}

inline bool
Filter::MatchMore(std::span<const std::byte> raw) const noexcept
{
	if (!http_status && hosts.empty() && http_uri_starts_with.empty())
		return true;

	try {
		const auto d = Net::Log::ParseDatagram(raw);

		if (http_status && !http_status(static_cast<uint16_t>(d.http_status)))
			return false;

		return MatchFilter(d.host, hosts) &&
			MatchHttpUriStartsWith(d.http_uri, http_uri_starts_with);
	} catch (...) {
		return false;
	}
}

bool
Filter::operator()(const SmallDatagram &d, std::span<const std::byte> raw) const noexcept
{
	return MatchFilter(d.site, sites) &&
		(type == Net::Log::Type::UNSPECIFIED ||
		 type == d.type) &&
		((since == Net::Log::TimePoint::min() &&
		  until == Net::Log::TimePoint::max()) ||
		 (d.HasTimestamp() &&
		  MatchTimestamp(d.timestamp, since, until))) &&
		MatchMore(raw);
}

bool
Filter::operator()(const Net::Log::Datagram &d) const noexcept
{
	return MatchFilter(d.site, sites) &&
		(type == Net::Log::Type::UNSPECIFIED ||
		 type == d.type) &&
		http_status(static_cast<uint16_t>(d.http_status)) &&
		((since == Net::Log::TimePoint::min() &&
		  until == Net::Log::TimePoint::max()) ||
		 (d.HasTimestamp() &&
		  MatchTimestamp(d.timestamp, since, until))) &&
		MatchHttpUriStartsWith(d.http_uri, http_uri_starts_with);
}
