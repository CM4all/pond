// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "AnyList.hxx"

#include <cstddef>
#include <utility>

#include <assert.h>
#include <stdint.h>

class Record;

/**
 * An iterator for records in the #Database.  While an instance
 * exists, the database must not be modified.
 */
class LightCursor {
	AnyRecordList list;

	const Record *next = nullptr;

public:
	explicit LightCursor(std::nullptr_t) noexcept {}

	explicit LightCursor(const AnyRecordList &_list) noexcept
		:list(_list) {}

	/**
	 * Rewind to the first record.
	 */
	void Rewind() noexcept {
		next = list.First();
	}

	/**
	 * If the pointed-to #Record has been deleted, rewind to the
	 * first record.
	 *
	 * @return true if the #Record has been deleted, false if the
	 * call was a no-op
	 */
	bool FixDeleted(uint64_t expected_id) noexcept;

	const Record *TimeLowerBound(Net::Log::TimePoint since) const noexcept {
		return list.TimeLowerBound(since);
	}

	void AddAppendListener(AppendListener &l) noexcept {
		list.AddAppendListener(l);
	}

	/**
	 * Does this instance point to a valid record?
	 */
	operator bool() const noexcept {
		return next != nullptr;
	}

	const Record &operator*() const noexcept {
		assert(next != nullptr);

		return *next;
	}

	const Record *operator->() const noexcept {
		assert(next != nullptr);

		return next;
	}

	/**
	 * Skip to the next record.
	 */
	LightCursor &operator++() noexcept {
		assert(next != nullptr);

		next = list.Next(*next);
		return *this;
	}

protected:
	const Record *First() const noexcept;

	void SetNext(const Record &record) noexcept {
		next = &record;
	}
};
