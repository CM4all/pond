/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <boost/intrusive/list_hook.hpp>

#include <assert.h>

class Database;
class Record;

class Cursor final
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>> {

	Database &database;
	const Record *next = nullptr;

public:
	explicit Cursor(Database &_database)
		:database(_database) {}

	void Rewind();

	void Follow();

	void OnAppend(const Record &record) {
		assert(!is_linked());
		assert(next == nullptr);

		next = &record;
	}

	void clear() {
		unlink();
		next = nullptr;
	}

	operator bool() const {
		return next != nullptr;
	}

	const Record &operator*() const {
		assert(next != nullptr);
		assert(!is_linked());

		return *next;
	}

	const Record *operator->() const {
		assert(next != nullptr);
		assert(!is_linked());

		return next;
	}

	Cursor &operator++();
};
