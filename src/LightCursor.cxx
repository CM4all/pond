/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "LightCursor.hxx"
#include "Database.hxx"
#include "Filter.hxx"

LightCursor::LightCursor(Database &_database, const Filter &filter) noexcept
	:all_records(filter.site.empty()
		     ? &_database.GetAllRecords()
		     : nullptr),
	 per_site_records(filter.site.empty()
			  ? nullptr
			  : &_database.GetPerSiteRecords(filter.site))
{
}

const Record *
LightCursor::First() const noexcept
{
	return all_records != nullptr
		? all_records->First()
		: per_site_records->First();
}

bool
LightCursor::FixDeleted(uint64_t expected_id) noexcept
{
	if (next == nullptr)
		return false;

	const auto *first = First();
	if (first != next && expected_id < first->GetId()) {
		next = first;
		return true;
	} else
		return false;
}

void
LightCursor::Rewind() noexcept
{
	next = First();
}

void
LightCursor::AddAppendListener(AppendListener &l) noexcept
{
	if (all_records != nullptr)
		all_records->AddAppendListener(l);
	else
		per_site_records->AddAppendListener(l);
}

LightCursor &
LightCursor::operator++() noexcept
{
	assert(next != nullptr);

	next = all_records != nullptr
		? all_records->Next(*next)
		: per_site_records->Next(*next);

	return *this;
}
