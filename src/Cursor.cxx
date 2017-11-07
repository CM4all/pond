/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Cursor.hxx"
#include "Database.hxx"
#include "Filter.hxx"

void
Cursor::Rewind()
{
	unlink();
	LightCursor::Rewind();

	if (*this)
		LightCursor::operator*().AddCursor(*this);
}

void
Cursor::Follow()
{
	assert(append_callback);

	if (!*this && !is_linked())
		LightCursor::Follow(*this);
}

void
Cursor::OnAppend(const Record &record)
{
	assert(!is_linked());
	assert(!*this);

	SetNext(record);
	record.AddCursor(*this);

	append_callback();
}

Cursor &
Cursor::operator++()
{
	assert(*this);
	assert(is_linked());

	unlink();

	LightCursor::operator++();
	if (*this)
		LightCursor::operator*().AddCursor(*this);

	return *this;
}
