/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Cursor.hxx"
#include "Database.hxx"
#include "Filter.hxx"

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
