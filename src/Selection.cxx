/*
 * Copyright 2017-2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
	while (*this && !filter(cursor->GetParsed()))
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

	if (!filter(record.GetParsed()))
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
