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

void
FilteredCursor::OnFilteredAppend() noexcept
{
	assert(filtered_append_callback);

	SkipMismatches();

	/* only invoke the callback if at least one new record matches
	   the filter */
	if (*this)
		filtered_append_callback();
}

FilteredCursor &
FilteredCursor::operator++() noexcept
{
	Cursor::operator++();
	SkipMismatches();
	return *this;
}
