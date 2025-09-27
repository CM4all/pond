// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Cursor.hxx"
#include "Filter.hxx"
#include "util/SharedLease.hxx"

#include <cassert>

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

	enum class State {
		/**
		 * At a mismatch currently (or unknown); need to call
		 * SkipMismatches() before using a #Record.
		 */
		MISMATCH,

		/**
		 * Like #MISMATCH, but ReverseSkipMismatches() must be
		 * used.
		 */
		MISMATCH_REVERSE,

		/**
		 * At a matching #Record.
		 */
		MATCH,

		/**
		 * At the end of the selection, no further records
		 * (yet).
		 */
		END,
	} state = State::END;

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
		State state;
	};

	/**
	 * Save the current position in an object, to be restored
	 * using Restore().
	 */
	Marker Mark() const noexcept {
		return {cursor.Mark(), state};
	}

	/**
	 * Restore a position saved by Mark().
	 */
	void Restore(Marker marker) noexcept {
		cursor.Restore(marker.cursor);
		state = marker.state;
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

	enum class UpdateResult {
		/**
		 * A #Record is available.
		 */
		READY,

		/**
		 * The selection has ended (but eventually, new
		 * matching records may be added).
		 */
		END,
	};

	/**
	 * Update internal state to make this object ready (e.g. skip
	 * mismatching records).
	 */
	[[nodiscard]]
	UpdateResult Update() noexcept;

	const Record &operator*() const noexcept {
		assert(state == State::MATCH);

		return *cursor;
	}

	const Record *operator->() const noexcept {
		assert(state == State::MATCH);

		return cursor.operator->();
	}

	/**
	 * Skip to the next record.
	 */
	Selection &operator++() noexcept;

	/**
	 * @return true if the record matched the filter
	 */
	bool OnAppend(const Record &record) noexcept;

private:
	[[gnu::pure]]
	bool IsDefined() const noexcept;

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
