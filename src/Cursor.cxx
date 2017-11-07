/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Cursor.hxx"
#include "Database.hxx"

Cursor::Cursor(Database &_database, BoundMethod<void()> _append_callback)
	:database(_database), append_callback(_append_callback)
{
}

void
Cursor::Rewind()
{
	unlink();
	next = database.First();
	if (next != nullptr)
		next->AddCursor(*this);
}

void
Cursor::Follow()
{
	assert(append_callback);

	if (next == nullptr && !is_linked())
		database.Follow(*this);
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
	next = database.Next(*next);
	if (next != nullptr)
		next->AddCursor(*this);

	return *this;
}
