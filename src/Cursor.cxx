/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Cursor.hxx"
#include "Database.hxx"
#include "Filter.hxx"

Cursor::Cursor(Database &_database, const Filter &filter,
	       BoundMethod<void()> _append_callback)
	:all_records(filter.site.empty()
		     ? &_database.GetAllRecords()
		     : nullptr),
	 per_site_records(filter.site.empty()
			  ? nullptr
			  : &_database.GetPerSiteRecords(filter.site)),
	 append_callback(_append_callback)
{
}

void
Cursor::Rewind()
{
	unlink();
	next = all_records != nullptr
		? all_records->First()
		: per_site_records->First();
	if (next != nullptr)
		next->AddCursor(*this);
}

void
Cursor::Follow()
{
	assert(append_callback);

	if (next == nullptr && !is_linked()) {
		if (all_records != nullptr)
			all_records->Follow(*this);
		else
			per_site_records->Follow(*this);
	}
}

void
Cursor::OnAppend(const Record &record)
{
	assert(!is_linked());
	assert(next == nullptr);

	next = &record;
	next->AddCursor(*this);

	append_callback();
}

Cursor &
Cursor::operator++()
{
	assert(next != nullptr);
	assert(is_linked());

	unlink();

	next = all_records != nullptr
		? all_records->Next(*next)
		: per_site_records->Next(*next);
	if (next != nullptr)
		next->AddCursor(*this);

	return *this;
}
