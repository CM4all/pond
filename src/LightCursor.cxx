/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "LightCursor.hxx"
#include "Database.hxx"
#include "Filter.hxx"

LightCursor::LightCursor(Database &_database, const Filter &filter) noexcept
{
	if (filter.site.empty())
		list = _database.GetAllRecords();
	else
		list = _database.GetPerSiteRecords(filter.site);
}

bool
LightCursor::FixDeleted(uint64_t expected_id) noexcept
{
	if (next == nullptr)
		return false;

	const auto *first = list.First();
	if (first != next && expected_id < first->GetId()) {
		next = first;
		return true;
	} else
		return false;
}
