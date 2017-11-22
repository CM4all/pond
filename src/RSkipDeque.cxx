/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "RSkipDeque.hxx"

#include <assert.h>

size_t
RecordSkipDeque::FindTimeOrGreaterIndex(size_t left_index, size_t right_index,
					uint64_t time) const noexcept
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

		const uint64_t middle_time = deque[middle_index].time;

		if (middle_time >= time)
			right_index = middle_index;
		else
			left_index = middle_index;
	}

	return left_index;
}

std::pair<const Record *, const Record *>
RecordSkipDeque::TimeRange(uint64_t since, uint64_t until) const noexcept
{
	if (deque.empty())
		return std::make_pair(nullptr, nullptr);

	size_t lower_index = since > 0
		? FindTimeOrGreaterIndex(0, deque.size() - 1, since)
		: 0;
	if (lower_index == SIZE_MAX)
		return std::make_pair(nullptr, nullptr);

	size_t upper_index = until > 0
		? FindTimeOrGreaterIndex(lower_index, deque.size() - 1, until)
		: deque.size() - 1;
	assert(upper_index < deque.size() || upper_index == SIZE_MAX);

	if (lower_index > 0)
		--lower_index;

	const Record *upper_record;

	if (upper_index == SIZE_MAX)
		upper_record = nullptr;
	else if (upper_index + 1 < deque.size()) {
		++upper_index;
		upper_record = &deque[upper_index].record;
	} else
		upper_record = the_last;

	return std::make_pair(&deque[lower_index].record,
			      upper_record);
}
