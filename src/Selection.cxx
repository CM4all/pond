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

inline bool
Selection::IsDefined() const noexcept
{
	return cursor && (!cursor->GetParsed().HasTimestamp() ||
			  cursor->GetParsed().timestamp - until_offset <= filter.timestamp.until);
}

inline Selection::UpdateResult
Selection::SkipMismatches() noexcept
{
	while (IsDefined()) {
		if (filter(cursor->GetParsed(), cursor->GetRaw())) {
			// found a match
			state = State::MATCH;
			return UpdateResult::READY;
		}

		++cursor;
	}

	/* no match found - clear the cursor so our "bool" operator
	   returns false */
	cursor.Clear();
	state = State::END;
	return UpdateResult::END;
}

inline bool
Selection::IsDefinedReverse() const noexcept
{
	return cursor && (!cursor->GetParsed().HasTimestamp() ||
			  cursor->GetParsed().timestamp + until_offset >= filter.timestamp.since);
}

inline Selection::UpdateResult
Selection::ReverseSkipMismatches() noexcept
{
	while (IsDefinedReverse()) {
		if (filter(cursor->GetParsed(), cursor->GetRaw())) {
			// found a match
			state = State::MATCH;
			return UpdateResult::READY;
		}

		--cursor;
	}

	/* no match found - clear the cursor so our "bool" operator
	   returns false */
	cursor.Clear();
	state = State::END;
	return UpdateResult::END;
}

bool
Selection::FixDeleted() noexcept
{
	if (!cursor.FixDeleted())
		return false;

	if (state == State::MATCH)
		state = State::MISMATCH;
	return true;
}

void
Selection::Rewind() noexcept
{
	assert(!cursor);

	if (filter.timestamp.HasSince()) {
		const auto *record = cursor.TimeLowerBound(filter.timestamp.since);
		if (record == nullptr)
			return;

		cursor.SetNext(*record);
	} else
		cursor.Rewind();

	state = State::MISMATCH;
}

void
Selection::SeekLast() noexcept
{
	assert(!cursor);

	const auto *record = cursor.LastUntil(filter.timestamp.until);
	if (record == nullptr)
		return;

	cursor.SetNext(*record);
	state = State::MISMATCH_REVERSE;
}

bool
Selection::OnAppend(const Record &record) noexcept
{
	assert(!cursor);

	if (!filter(record.GetParsed(), record.GetRaw()))
		return false;

	cursor.OnAppend(record);
	state = State::MATCH;
	return true;
}

Selection::UpdateResult
Selection::Update() noexcept
{
	switch (state) {
	case State::MISMATCH:
		return SkipMismatches();

	case State::MISMATCH_REVERSE:
		return ReverseSkipMismatches();

	case State::MATCH:
		assert(cursor);
		return UpdateResult::READY;

	case State::END:
		return UpdateResult::END;
	}

	assert(false);
	std::unreachable();
}

Selection &
Selection::operator++() noexcept
{
	cursor.operator++();
	state = State::MISMATCH;
	return *this;
}
