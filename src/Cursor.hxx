/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "LightCursor.hxx"
#include "util/BindMethod.hxx"

#include <boost/intrusive/list.hpp>

#include <assert.h>

/**
 * An iterator for records in the #Database.  While an instance
 * exists, the database may be modified, because FixDeleted() will
 * take care for cleaning up invalid pointers.
 */
class Cursor final
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>,
	  LightCursor {

	BoundMethod<void() noexcept> append_callback;

	uint64_t id;

public:
	explicit Cursor(std::nullptr_t n) noexcept
		:LightCursor(n) {}

	Cursor(Database &_database, const Filter &filter,
	       BoundMethod<void() noexcept> _append_callback) noexcept
		:LightCursor(_database, filter),
		 append_callback(_append_callback) {}

	LightCursor ToLightCursor() const noexcept {
		return *this;
	}

	/**
	 * If the pointed-to #Record has been deleted, rewind to the
	 * first record.
	 */
	void FixDeleted() noexcept;

	/**
	 * Rewind to the first record.
	 */
	void Rewind() noexcept;

	/**
	 * Enable "follow" mode: the #Database will call OnAppend() as
	 * soon as a new record gets added.
	 */
	void Follow() noexcept;

	/**
	 * Callback invoked by the #Database.
	 */
	void OnAppend(const Record &record) noexcept;

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
};

typedef boost::intrusive::list<Cursor,
			       boost::intrusive::constant_time_size<false>> CursorList;
