// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/log/Chrono.hxx"

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

	void Compress() noexcept {
		deque.shrink_to_fit();
	}

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
	[[gnu::pure]]
	const Record *TimeLowerBound(Net::Log::TimePoint since) const noexcept;
};
