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
	if (next == nullptr && !is_linked())
		database.Follow(*this);
}

Cursor &
Cursor::operator++()
{
	assert(next != nullptr);
	assert(!is_linked());

	next = database.Next(*next);
	return *this;
}
