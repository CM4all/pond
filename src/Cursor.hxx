/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "LightCursor.hxx"

#include <boost/intrusive/list.hpp>

#include <assert.h>

/**
 * An iterator for records in the #Database.  While an instance
 * exists, the database may be modified, because FixDeleted() will
 * take care for cleaning up invalid pointers.
 */
class Cursor
	: LightCursor {

	uint64_t id;

public:
	explicit Cursor(std::nullptr_t n) noexcept
		:LightCursor(n) {}

	Cursor(Database &_database, const Filter &filter) noexcept
		:LightCursor(_database, filter) {}

	LightCursor ToLightCursor() const noexcept {
		return *this;
	}

	/**
	 * If the pointed-to #Record has been deleted, rewind to the
	 * first record.
	 *
	 * @return true if the #Record has been deleted, false if the
	 * call was a no-op
	 */
	bool FixDeleted() noexcept;

	using LightCursor::AddAppendListener;

	/**
	 * Rewind to the first record.
	 */
	void Rewind() noexcept;

	bool operator==(const Cursor &other) const noexcept {
		return LightCursor::operator==(other);
	}

	bool operator!=(const Cursor &other) const noexcept {
		return !(*this == other);
	}

	/**
	 * Does this instance point to a valid record?
	 */
	using LightCursor::operator bool;
	using LightCursor::operator*;
	using LightCursor::operator->;

	/**
	 * Skip to the next record.
	 */
	Cursor &operator++() noexcept;

	void OnAppend(const Record &record) noexcept;
};

typedef boost::intrusive::list<Cursor,
			       boost::intrusive::constant_time_size<false>> CursorList;
