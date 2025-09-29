// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "AnyList.hxx"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

class Record;

/**
 * An iterator for records in the #Database.  While an instance
 * exists, the database must not be modified.
 */
class LightCursor {
	AnyRecordList list;

	const Record *next = nullptr;

public:
	explicit constexpr LightCursor(const AnyRecordList &_list) noexcept
		:list(_list) {}

	/**
	 * Clear the current record, as if we had arrived at the end
	 * of the list.
	 */
	void Clear() noexcept {
		next = nullptr;
	}

	/**
	 * Rewind to the first record.
	 */
	void Rewind() noexcept {
		next = list.First();
	}

	void SeekLast() noexcept {
		next = list.Last();
	}

	/**
	 * Opaque struct for Mark() and Restore().
	 */
	struct Marker {
		const Record *record;
	};

	/**
	 * Save the current position in an object, to be restored
	 * using Restore().
	 */
	Marker Mark() const noexcept {
		return {next};
	}

	/**
	 * Restore a position saved by Mark().
	 */
	void Restore(Marker marker) noexcept {
		next = marker.record;
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

	const Record *LastUntil(Net::Log::TimePoint until) const noexcept {
		return list.LastUntil(until);
	}

	void AddAppendListener(AppendListener &l) noexcept {
		list.AddAppendListener(l);
	}

	/**
	 * Does this instance point to a valid record?
	 */
	constexpr operator bool() const noexcept {
		return next != nullptr;
	}

	constexpr const Record &operator*() const noexcept {
		assert(next != nullptr);

		return *next;
	}

	constexpr const Record *operator->() const noexcept {
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

	/**
	 * Skip to the previous record.
	 */
	LightCursor &operator--() noexcept {
		assert(next != nullptr);

		next = list.Previous(*next);
		return *this;
	}

protected:
	constexpr void SetNext(const Record &record) noexcept {
		next = &record;
	}
};
