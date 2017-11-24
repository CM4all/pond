/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Selection.hxx"
#include "Filter.hxx"
#include "Record.hxx"

void
Selection::SkipMismatches() noexcept
{
	while (*this && !filter((*this)->GetParsed()))
		++*this;
}

bool
Selection::FixDeleted() noexcept
{
	if (!Cursor::FixDeleted())
		return false;

	SkipMismatches();
	return true;
}

void
Selection::Rewind() noexcept
{
	Cursor::Rewind();
	SkipMismatches();
}

bool
Selection::OnAppend(const Record &record) noexcept
{
	assert(!*this);

	if (!filter(record.GetParsed()))
		return false;

	Cursor::OnAppend(record);
	return true;
}

Selection::operator bool() const noexcept
{
	return Cursor::operator bool() && (*this)->GetId() <= end_id;
}

Selection &
Selection::operator++() noexcept
{
	Cursor::operator++();
	SkipMismatches();
	return *this;
}
