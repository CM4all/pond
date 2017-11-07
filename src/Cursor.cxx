/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Cursor.hxx"
#include "Database.hxx"

Cursor::Cursor(Database &_database, BoundMethod<void()> _append_callback)
	:all_records(_database.GetAllRecords()),
	 append_callback(_append_callback)
{
}

void
Cursor::Rewind()
{
	unlink();
	next = all_records.First();
	if (next != nullptr)
		next->AddCursor(*this);
}

void
Cursor::Follow()
{
	assert(append_callback);

	if (next == nullptr && !is_linked())
		all_records.Follow(*this);
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
	next = all_records.Next(*next);
	if (next != nullptr)
		next->AddCursor(*this);

	return *this;
}
