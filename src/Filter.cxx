/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Filter.hxx"
#include "net/log/Datagram.hxx"

gcc_pure
static bool
MatchFilter(const char *value, const std::set<std::string> &filter) noexcept
{
	return filter.empty() || (value != nullptr &&
				  filter.find(value) != filter.end());
}

static bool
MatchTimestamp(uint64_t timestamp, uint64_t since, uint64_t until)
{
	return timestamp >= since && (until == 0 || timestamp <= until);
}

bool
Filter::operator()(const Net::Log::Datagram &d) const noexcept
{
	return MatchFilter(d.site, sites) &&
		(type == Net::Log::Type::UNSPECIFIED ||
		 type == d.type) &&
		((since == 0 && until == 0) ||
		 (d.valid_timestamp &&
		  MatchTimestamp(d.timestamp, since, until)));
}
