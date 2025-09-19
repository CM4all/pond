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
	 * #cursor.  The lease ensured that the object does not get
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

	bool FixDeleted() noexcept;
	void Rewind() noexcept;

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

	bool OnAppend(const Record &record) noexcept;

private:
	void SkipMismatches() noexcept;
};
