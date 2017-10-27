/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Cursor.hxx"
#include "Database.hxx"

void
Cursor::Rewind()
{
	unlink();
	next = database.First();
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
	append_callback();
}

Cursor &
Cursor::operator++()
{
	assert(next != nullptr);
	assert(!is_linked());

	next = database.Next(*next);
	return *this;
}
