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

#pragma once

#include "net/log/Chrono.hxx"
#include "util/Compiler.h"

#include <deque>

#include <stdint.h>

class Record;

/**
 * A sort of "index" for the #Record time.  This is a very simple and
 * naive implementation: a std::deque holds a list of every 4096th
 * record.  With a binary search, we can limit the range where we will
 * traverse the #Record linked list.
 */
class RecordSkipDeque {
	/**
	 * An item in the skip list.
	 */
	struct Item {
		/**
		 * The first (and hopefully earliest) #Record in this
		 * group.
		 */
		const Record &record;

		/**
		 * This record's id.  This struct must contain a copy
		 * because it is needed by FixDeleted(), which gets
		 * called after records have been disposed, and it's
		 * impossible to dereference the #Record.
		 */
		const uint64_t id;

		/**
		 * This earliest time stamp in this group.  Due to
		 * timing glitches, the earliest time stamp may not be
		 * the first one.
		 */
		Net::Log::TimePoint time;

		explicit Item(const Record &_record) noexcept;
	};

	using Deque = std::deque<Item>;

	/**
	 * The actual skip list of #Record instances.
	 */
	Deque deque;

	/**
	 * The last record in the list, which however may not be in
	 * the #deque.  It is needed to find the real end of the list
	 * in TimeRange().
	 */
	const Record *the_last = nullptr;

	/**
	 * The distance between two records in the #deque.
	 */
	static constexpr uint64_t SKIP_COUNT = 4096;

public:
	RecordSkipDeque() = default;

	RecordSkipDeque(const RecordSkipDeque &) = delete;
	RecordSkipDeque &operator=(const RecordSkipDeque &) = delete;

	/**
	 * Remove pointers to deleted #Record instances from this
	 * container.
	 */
	void FixDeleted(const Record &first) noexcept;

	void UpdateNew(const Record &last) noexcept;

	/**
	 * Find the records spanning the given time range.  Returns a
	 * pair of #Record pointers denoting the first record inside
	 * the range and the first record after the range.  The first
	 * is `nullptr` if all records are older, and the second is
	 * `nullptr` if there are no records after the end of the
	 * range.
	 */
	gcc_pure
	const Record *TimeLowerBound(Net::Log::TimePoint since) const noexcept;
};
