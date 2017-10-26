/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Cursor.hxx"
#include "Database.hxx"

void
Cursor::Rewind()
{
	next = database.First();
}

Cursor &
Cursor::operator++()
{
	assert(next != nullptr);

	next = database.Next(*next);
	return *this;
}
