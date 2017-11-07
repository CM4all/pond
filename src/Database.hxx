/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "RList.hxx"
#include "Cursor.hxx"

template<typename T> struct ConstBuffer;

class Database {
	static constexpr size_t max_records = 1024 * 1024;

	RecordList all_records;

public:
	Database() = default;
	~Database();

	Database(const Database &) = delete;
	Database &operator=(const Database &) = delete;

	RecordList &GetAllRecords() {
		return all_records;
	}

	const Record &Emplace(ConstBuffer<uint8_t> raw);

	/**
	 * Remove and free the given record.
	 */
	void Dispose(Record *record);
};
