/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "LightCursor.hxx"

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

	explicit Cursor(const AnyRecordList &_list) noexcept
		:LightCursor(_list) {}

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

	using LightCursor::TimeRange;
	using LightCursor::AddAppendListener;

	void SetNext(const Record &record) noexcept;

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
