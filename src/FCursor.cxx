/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "FCursor.hxx"
#include "Filter.hxx"
#include "Record.hxx"

void
FilteredCursor::SkipMismatches() noexcept
{
	while (*this && !filter((*this)->GetParsed()))
		++*this;
}

bool
FilteredCursor::FixDeleted() noexcept
{
	if (!Cursor::FixDeleted())
		return false;

	SkipMismatches();
	return true;
}

void
FilteredCursor::Rewind() noexcept
{
	Cursor::Rewind();
	SkipMismatches();
}

bool
FilteredCursor::OnAppend(const Record &record) noexcept
{
	assert(!*this);

	if (!filter(record.GetParsed()))
		return false;

	Cursor::OnAppend(record);
	return true;
}

FilteredCursor &
FilteredCursor::operator++() noexcept
{
	Cursor::operator++();
	SkipMismatches();
	return *this;
}
