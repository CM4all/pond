/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "LightCursor.hxx"
#include "Database.hxx"
#include "Filter.hxx"

LightCursor::LightCursor(Database &_database, const Filter &filter)
	:all_records(filter.site.empty()
		     ? &_database.GetAllRecords()
		     : nullptr),
	 per_site_records(filter.site.empty()
			  ? nullptr
			  : &_database.GetPerSiteRecords(filter.site))
{
}

void
LightCursor::Rewind()
{
	next = all_records != nullptr
		? all_records->First()
		: per_site_records->First();
}

void
LightCursor::Follow(Cursor &cursor)
{
	if (all_records != nullptr)
		all_records->Follow(cursor);
	else
		per_site_records->Follow(cursor);
}

LightCursor &
LightCursor::operator++()
{
	assert(next != nullptr);

	next = all_records != nullptr
		? all_records->Next(*next)
		: per_site_records->Next(*next);

	return *this;
}
