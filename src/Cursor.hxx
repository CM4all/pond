/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <assert.h>

class Database;
class Record;

class Cursor {
	const Database &database;
	const Record *next = nullptr;

public:
	explicit Cursor(const Database &_database)
		:database(_database) {}

	void Rewind();

	void clear() {
		next = nullptr;
	}

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

	Cursor &operator++();
};
