/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Cursor.hxx"

#include <boost/intrusive/list.hpp>

#include <assert.h>

/**
 * A wrapper for #Cursor which applies a #Filter.
 */
class FilteredCursor : Cursor {
	const Filter &filter;

	BoundMethod<void() noexcept> filtered_append_callback;

public:
	FilteredCursor(Database &_database, const Filter &_filter,
		       BoundMethod<void()> _append_callback) noexcept
		:Cursor(_database, _filter, BIND_THIS_METHOD(OnFilteredAppend)),
		 filter(_filter), filtered_append_callback(_append_callback) {}

	using Cursor::ToLightCursor;

	bool FixDeleted() noexcept;
	void Rewind() noexcept;

	using Cursor::Follow;

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

private:
	void SkipMismatches() noexcept;
	void OnFilteredAppend() noexcept;
};

typedef boost::intrusive::list<Cursor,
			       boost::intrusive::constant_time_size<false>> CursorList;
