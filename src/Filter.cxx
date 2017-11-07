/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Filter.hxx"
#include "net/log/Datagram.hxx"

gcc_pure
static bool
MatchFilter(const char *value, const std::string &filter) noexcept
{
	return filter.empty() || (value != nullptr && filter == value);
}

bool
Filter::operator()(const Net::Log::Datagram &d) const noexcept
{
	return MatchFilter(d.site, site);
}
