/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Cursor.hxx"

/**
 * A wrapper for #Cursor which applies a #Filter.
 */
class Selection {
	Cursor cursor;

	const Filter &filter;

	const uint64_t end_id = UINT64_MAX;

public:
	Selection(Database &_database, const Filter &_filter) noexcept
		:cursor(_database, _filter),
		 filter(_filter) {}

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
