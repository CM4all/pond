/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Cursor.hxx"

/**
 * A wrapper for #Cursor which applies a #Filter.
 */
class Selection : Cursor {
	const Filter &filter;

	const uint64_t end_id = UINT64_MAX;

public:
	Selection(Database &_database, const Filter &_filter) noexcept
		:Cursor(_database, _filter),
		 filter(_filter) {}

	using Cursor::ToLightCursor;

	bool FixDeleted() noexcept;
	void Rewind() noexcept;

	using Cursor::AddAppendListener;

	using Cursor::operator==;
	using Cursor::operator!=;

	operator bool() const noexcept;

	using Cursor::operator*;
	using Cursor::operator->;

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
