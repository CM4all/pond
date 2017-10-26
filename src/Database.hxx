/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Cursor.hxx"
#include "net/log/Datagram.hxx"
#include "util/ConstBuffer.hxx"

#include <boost/intrusive/list.hpp>

#include <memory>

typedef boost::intrusive::list<Cursor,
			       boost::intrusive::constant_time_size<false>> CursorList;

class Record final
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> {

	std::unique_ptr<uint8_t[]> raw;
	size_t raw_size;

	Net::Log::Datagram parsed;

public:
	/**
	 * Throws Net::Log::ProtocolError on error.
	 */
	explicit Record(ConstBuffer<uint8_t> _raw);

	Record(const Record &) = delete;
	Record &operator=(const Record &) = delete;

	ConstBuffer<void> GetRaw() const {
		return {raw.get(), raw_size};
	}

	const Net::Log::Datagram &GetParsed() const {
		return parsed;
	}
};

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
