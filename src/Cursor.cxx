/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Cursor.hxx"
#include "Record.hxx"

bool
Cursor::FixDeleted() noexcept
{
	if (*this && LightCursor::FixDeleted(id)) {
		id = LightCursor::operator*().GetId();
		return true;
	} else
		return false;
}

void
Cursor::SetNext(const Record &record) noexcept
{
	LightCursor::SetNext(record);
	id = record.GetId();
}

void
Cursor::Rewind() noexcept
{
	LightCursor::Rewind();

	if (*this)
		id = LightCursor::operator*().GetId();
}

void
Cursor::OnAppend(const Record &record) noexcept
{
	assert(!*this);

	SetNext(record);
	id = record.GetId();
}

Cursor &
Cursor::operator++() noexcept
{
	assert(*this);

	LightCursor::operator++();
	if (*this)
		id = LightCursor::operator*().GetId();

	return *this;
}
