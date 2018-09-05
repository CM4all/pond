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

	/**
	 * Find the records spanning the given time range.  Returns a
	 * pair of #Record pointers denoting the first record inside
	 * the range and the first record after the range.  The first
	 * is `nullptr` if all records are older, and the second is
	 * `nullptr` if there are no records after the end of the
	 * range.
	 */
	gcc_pure
	std::pair<const Record *, const Record *> TimeRange(uint64_t since,
							    uint64_t until) const noexcept;

private:
	/**
	 * Find the lowest `std::deque` index which has the given time
	 * stamp or is newer.  Returns SIZE_MAX if all time stamps are
	 * older.
	 */
	gcc_pure
	size_t FindTimeOrGreaterIndex(size_t left_index, size_t right_index,
				      uint64_t time) const noexcept;
};
