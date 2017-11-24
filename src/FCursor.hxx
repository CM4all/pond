/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Cursor.hxx"

/**
 * A wrapper for #Cursor which applies a #Filter.
 */
class FilteredCursor : Cursor {
	const Filter &filter;

public:
	FilteredCursor(Database &_database, const Filter &_filter) noexcept
		:Cursor(_database, _filter),
		 filter(_filter) {}

	using Cursor::ToLightCursor;

	bool FixDeleted() noexcept;
	void Rewind() noexcept;

	using Cursor::AddAppendListener;

	using Cursor::operator==;
	using Cursor::operator!=;

	using Cursor::operator bool;
	using Cursor::operator*;
	using Cursor::operator->;

	/**
	 * Skip to the next record.
	 */
	FilteredCursor &operator++() noexcept;

	FilteredCursor &operator+=(size_t n) noexcept {
		while (n-- > 0)
			operator++();
		return *this;
	}

	bool OnAppend(const Record &record) noexcept;

private:
	void SkipMismatches() noexcept;
};
