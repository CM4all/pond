// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Cursor.hxx"
#include "Filter.hxx"
#include "util/SharedLease.hxx"

/**
 * A wrapper for #Cursor which applies a #Filter.
 */
class Selection {
	Cursor cursor;

	Filter filter;

	/**
	 * A lease for the #Datbase::PerSite that may be referenced by
	 * #cursor.  The lease ensures that the object does not get
	 * freed as long as this #Selection exists.
	 */
	SharedLease lease;

public:
	template<typename F, typename L>
	Selection(const AnyRecordList &_list, F &&_filter,
		  L &&_lease) noexcept
		:cursor(_list),
		 filter(std::forward<F>(_filter)),
		 lease(std::forward<L>(_lease)) {}

	/**
	 * Opaque struct for Mark() and Restore().
	 */
	struct Marker {
		Cursor::Marker cursor;
	};

	/**
	 * Save the current position in an object, to be restored
	 * using Restore().
	 */
	Marker Mark() const noexcept {
		return {cursor.Mark()};
	}

	/**
	 * Restore a position saved by Mark().
	 */
	void Restore(Marker marker) noexcept {
		cursor.Restore(marker.cursor);
	}

	/**
	 * If the pointed-to #Record has been deleted, rewind to the
	 * first record.
	 *
	 * @return true if the #Record has been deleted, false if the
	 * call was a no-op
	 */
	bool FixDeleted() noexcept;

	/**
	 * Move to the first matching record.
	 */
	void Rewind() noexcept;

	/**
	 * Move to the last matching record.
	 */
	void SeekLast() noexcept;

	void AddAppendListener(AppendListener &l) noexcept {
		cursor.AddAppendListener(l);
	}

	operator bool() const noexcept;

	const Record &operator*() const noexcept {
		return *cursor;
	}

	const Record *operator->() const noexcept {
		return cursor.operator->();
	}

	/**
	 * Skip to the next record.
	 */
	Selection &operator++() noexcept;

	Selection &operator+=(size_t n) noexcept {
		while (n-- > 0)
			operator++();
		return *this;
	}

	/**
	 * @return true if the record matched the filter
	 */
	bool OnAppend(const Record &record) noexcept;

private:
	/**
	 * Skip all records that do not match the filter and move
	 * forward on until matching record was found (or until there
	 * are no further records).
	 */
	void SkipMismatches() noexcept;

	bool IsDefinedReverse() const noexcept;

	/**
	 * Like SkipMismatches(), but move backwards.
	 */
	void ReverseSkipMismatches() noexcept;
};
