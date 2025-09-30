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
Selection::SkipMismatches(unsigned max_steps) noexcept
{
	while (IsDefined()) {
		if (max_steps-- == 0)
			return UpdateResult::AGAIN;

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
Selection::ReverseSkipMismatches(unsigned max_steps) noexcept
{
	while (IsDefinedReverse()) {
		if (max_steps-- == 0)
			return UpdateResult::AGAIN;

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
Selection::Update(unsigned max_steps) noexcept
{
	switch (state) {
	case State::MISMATCH:
		return SkipMismatches(max_steps);

	case State::MISMATCH_REVERSE:
		return ReverseSkipMismatches(max_steps);

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
