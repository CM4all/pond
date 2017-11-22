/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Record.hxx"
#include "util/Compiler.h"

#include <deque>

/**
 * A sort of "index" for the #Record time.  This is a very simple and
 * naive implementation: a std::deque holds a list of every 4096th
 * record.  With a binary search, we can limit the range where we will
 * traverse the #Record linked list.
 */
class RecordSkipDeque {
	struct Item {
		const Record &record;
		uint64_t id;
		uint64_t time;

		explicit Item(const Record &_record)
			:record(_record), id(record.GetId()),
			 time(record.GetParsed().timestamp) {}
	};

	using Deque = std::deque<Item>;

	Deque deque;

	const Record *the_last = nullptr;

	static constexpr uint64_t SKIP_COUNT = 4096;

public:
	/**
	 * Remove pointers to deleted #Record instances from this
	 * container.
	 */
	void FixDeleted(const Record &first) noexcept {
		const auto min_id = first.GetId();
		while (!deque.empty() && deque.front().id < min_id)
			deque.pop_front();

		if (deque.empty())
			the_last = nullptr;
	}

	void UpdateNew(const Record &last) noexcept {
		the_last = &last;

		if (last.GetParsed().valid_timestamp &&
		    (deque.empty() ||
		     last.GetId() >= deque.back().id + SKIP_COUNT))
			deque.emplace_back(last);
	}

	gcc_pure
	std::pair<const Record *, const Record *> TimeRange(uint64_t since,
							    uint64_t until) const noexcept;

private:
	size_t FindTimeOrGreaterIndex(size_t left_index, size_t right_index,
				      uint64_t time) const noexcept;
};
