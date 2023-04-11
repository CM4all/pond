// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Selection.hxx"
#include "Record.hxx"

/**
 * Stop searching for matching time stamps for this duration after the
 * given "until" time stamp.  This shall avoid stopping too early when
 * there is jitter.
 */
static constexpr Net::Log::Duration until_offset = std::chrono::seconds(10);

void
Selection::SkipMismatches() noexcept
{
	while (*this && !filter(cursor->GetParsed(), cursor->GetRaw()))
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

	if (filter.since != Net::Log::TimePoint::min()) {
		const auto *record = cursor.TimeLowerBound(filter.since);
		if (record == nullptr)
			return;

		cursor.SetNext(*record);
	} else
		cursor.Rewind();

	SkipMismatches();
}

bool
Selection::OnAppend(const Record &record) noexcept
{
	assert(!*this);

	if (!filter(record.GetParsed(), record.GetRaw()))
		return false;

	cursor.OnAppend(record);
	return true;
}

Selection::operator bool() const noexcept
{
	return cursor && (!cursor->GetParsed().HasTimestamp() ||
			  cursor->GetParsed().timestamp - until_offset <= filter.until);
}

Selection &
Selection::operator++() noexcept
{
	cursor.operator++();
	SkipMismatches();
	return *this;
}
