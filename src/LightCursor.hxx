/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <assert.h>

struct Filter;
class Database;
class FullRecordList;
class PerSiteRecordList;
class Record;
class Cursor;

/**
 * An iterator for records in the #Database.  While an instance
 * exists, the database must not be modified.
 */
class LightCursor {
	FullRecordList *const all_records = nullptr;
	PerSiteRecordList *const per_site_records = nullptr;

	const Record *next = nullptr;

public:
	LightCursor(Database &_database, const Filter &filter);

	/**
	 * Rewind to the first record.
	 */
	void Rewind();

	/**
	 * Does this instance point to a valid record?
	 */
	operator bool() const {
		return next != nullptr;
	}

	const Record &operator*() const {
		assert(next != nullptr);

		return *next;
	}

	const Record *operator->() const {
		assert(next != nullptr);

		return next;
	}

	/**
	 * Skip to the next record.
	 */
	LightCursor &operator++();

protected:
	const Record *First() const noexcept;

	void SetNext(const Record &record) {
		next = &record;
	}

	void Follow(Cursor &cursor);
};
