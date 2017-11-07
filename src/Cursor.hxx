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
 * exists, the database may be modified, because
 * #Record::DisplaceCursors() will take care for cleaning up
 * soon-to-be-invalid pointers.
 */
class Cursor final
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>,
	  LightCursor {

	BoundMethod<void()> append_callback;

public:
	Cursor(Database &_database, const Filter &filter,
	       BoundMethod<void()> _append_callback)
		:LightCursor(_database, filter),
		 append_callback(_append_callback) {}

	LightCursor ToLightCursor() const {
		return *this;
	}

	/**
	 * Rewind to the first record.
	 */
	void Rewind();

	/**
	 * Enable "follow" mode: the #Database will call OnAppend() as
	 * soon as a new record gets added.
	 */
	void Follow();

	/**
	 * Callback invoked by the #Database.
	 */
	void OnAppend(const Record &record);

	/**
	 * Does this instance point to a valid record?
	 */
	using LightCursor::operator bool;

	const Record &operator*() const {
		assert(is_linked());

		return LightCursor::operator*();
	}

	const Record *operator->() const {
		assert(is_linked());

		return LightCursor::operator->();
	}

	/**
	 * Skip to the next record.
	 */
	Cursor &operator++();
};

typedef boost::intrusive::list<Cursor,
			       boost::intrusive::constant_time_size<false>> CursorList;
