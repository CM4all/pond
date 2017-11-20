/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Cursor.hxx"
#include "Database.hxx"
#include "Filter.hxx"

void
Cursor::FixDeleted()
{
	if (LightCursor::FixDeleted(id)) {
		assert(!is_linked());
		id = LightCursor::operator*().GetId();
	}
}

void
Cursor::Rewind()
{
	unlink();
	LightCursor::Rewind();

	if (*this)
		id = LightCursor::operator*().GetId();
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
	id = record.GetId();

	append_callback();
}

Cursor &
Cursor::operator++()
{
	assert(*this);

	LightCursor::operator++();
	if (*this)
		id = LightCursor::operator*().GetId();

	return *this;
}
