/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Selection.hxx"
#include "Filter.hxx"
#include "Record.hxx"

void
Selection::SkipMismatches() noexcept
{
	while (cursor && !filter(cursor->GetParsed()))
		++cursor;
}

bool
Selection::FixDeleted() noexcept
{
	if (!cursor.FixDeleted())
		return false;

	SkipMismatches();
	return true;
}

void
Selection::Rewind() noexcept
{
	assert(!cursor);
	assert(end_id == UINT64_MAX);

	if (filter.since > 0 || filter.until > 0) {
		const auto tr = cursor.TimeRange(filter.since, filter.until);
		if (tr.first == nullptr)
			return;

		cursor.SetNext(*tr.first);

		if (tr.second != nullptr)
			end_id = tr.second->GetId();
	} else
		cursor.Rewind();

	SkipMismatches();
}

bool
Selection::OnAppend(const Record &record) noexcept
{
	assert(!*this);

	if (!filter(record.GetParsed()))
		return false;

	cursor.OnAppend(record);
	return true;
}

Selection::operator bool() const noexcept
{
	return cursor && cursor->GetId() <= end_id;
}

Selection &
Selection::operator++() noexcept
{
	cursor.operator++();
	SkipMismatches();
	return *this;
}
