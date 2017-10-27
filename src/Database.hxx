/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Record.hxx"
#include "Cursor.hxx"

#include <boost/intrusive/list.hpp>

template<typename T> struct ConstBuffer;

class Database {
	typedef boost::intrusive::list<Record,
				       boost::intrusive::constant_time_size<false>> RecordList;

	RecordList records;

	CursorList follow_cursors;

public:
	Database() = default;
	~Database();

	Database(const Database &) = delete;
	Database &operator=(const Database &) = delete;

	RecordList::const_iterator begin() const {
			return records.begin();
	}

	RecordList::const_iterator end() const {
			return records.end();
	}

	const Record &Emplace(ConstBuffer<uint8_t> raw);

	const Record *First() const {
		return records.empty() ? nullptr : &records.front();
	}

	const Record *Next(const Record &current) const {
		auto i = records.iterator_to(current);
		++i;
		return i == records.end()
			? nullptr
			: &*i;
	}

	void Follow(Cursor &cursor) {
		follow_cursors.push_back(cursor);
	}
};
