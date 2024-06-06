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
	if (!NeedMore())
		return true;

	try {
		const auto d = Net::Log::ParseDatagram(raw);

		if (!http_status(static_cast<uint16_t>(d.http_status)))
			return false;

		return MatchFilter(d.host, hosts) &&
			MatchFilter(d.generator, generators) &&
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
		timestamp(d) &&
		MatchMore(raw);
}

bool
Filter::operator()(const Net::Log::Datagram &d) const noexcept
{
	return MatchFilter(d.site, sites) &&
		(type == Net::Log::Type::UNSPECIFIED ||
		 type == d.type) &&
		http_status(static_cast<uint16_t>(d.http_status)) &&
		timestamp(d) &&
		MatchFilter(d.host, hosts) &&
		MatchFilter(d.generator, generators) &&
		MatchHttpUriStartsWith(d.http_uri, http_uri_starts_with);
}
