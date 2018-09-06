/*
 * Copyright 2017-2018 Content Management AG
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

#include "RSkipDeque.hxx"
#include "Record.hxx"

#include <assert.h>

inline
RecordSkipDeque::Item::Item(const Record &_record) noexcept
	:record(_record), id(record.GetId()),
	 time(record.GetParsed().timestamp) {}

void
RecordSkipDeque::FixDeleted(const Record &first) noexcept
{
	const auto min_id = first.GetId();
	while (!deque.empty() && deque.front().id < min_id)
		deque.pop_front();

	if (deque.empty())
		the_last = nullptr;
}

void
RecordSkipDeque::UpdateNew(const Record &last) noexcept
{
	the_last = &last;

	if (last.GetParsed().HasTimestamp()) {
		if (deque.empty() ||
		    last.GetId() >= deque.back().id + SKIP_COUNT)
			/* create a new skip item */
			deque.emplace_back(last);
		else if (!deque.empty() &&
			 last.GetParsed().timestamp < deque.back().time)
			/* the new time stamp is older; update the
			   current item's time stamp */
			deque.back().time = last.GetParsed().timestamp;
	}
}

size_t
RecordSkipDeque::FindTimeOrGreaterIndex(size_t left_index, size_t right_index,
					Net::Log::TimePoint time) const noexcept
{
	assert(!deque.empty());
	assert(left_index <= right_index);
	assert(right_index < deque.size());

	if (time <= deque[left_index].time)
		return left_index;

	if (time > deque[right_index].time)
		return SIZE_MAX;

	while (left_index < right_index) {
		// TODO: interpolate
		const size_t middle_index = (left_index + right_index) / 2;
		if (middle_index == left_index)
			break;

		const Net::Log::TimePoint middle_time = deque[middle_index].time;

		if (middle_time >= time)
			right_index = middle_index;
		else
			left_index = middle_index;
	}

	return left_index;
}

const Record *
RecordSkipDeque::TimeLowerBound(Net::Log::TimePoint since) const noexcept
{
	assert(since != Net::Log::TimePoint::min());

	if (deque.empty())
		return nullptr;

	size_t lower_index =
		FindTimeOrGreaterIndex(0, deque.size() - 1, since);
	if (lower_index == SIZE_MAX)
		return nullptr;

	if (lower_index > 0)
		--lower_index;

	return &deque[lower_index].record;
}
