// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "RSkipDeque.hxx"
#include "Record.hxx"

#include <algorithm>

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

const Record *
RecordSkipDeque::TimeLowerBound(Net::Log::TimePoint since) const noexcept
{
	assert(since != Net::Log::TimePoint::min());

	auto i = std::lower_bound(deque.begin(), deque.end(), since, [](const auto &a, const auto b){
		return a.time < b;
	});

	if (i != deque.begin())
		--i;
	else if (i == deque.end())
		return nullptr;

	return &i->record;
}
