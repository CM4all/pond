/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "RList.hxx"
#include "system/LargeAllocation.hxx"

#include <unordered_map>

template<typename T> struct ConstBuffer;

class Database {
	LargeAllocation allocation;

	uint64_t last_id = 0;

	/**
	 * A chronological list of all records.  This list "owns" the
	 * allocated #Record instances.
	 */
	FullRecordList all_records;

	/**
	 * A chronological list for each site.  This list does not
	 * "own" the #Record instances, it only points to those owned
	 * by #all_records.
	 */
	// TODO: purge empty lists eventually
	std::unordered_map<std::string, PerSiteRecordList> per_site_records;

public:
	explicit Database(size_t max_size);
	~Database();

	Database(const Database &) = delete;
	Database &operator=(const Database &) = delete;

	FullRecordList &GetAllRecords() {
		return all_records;
	}

	PerSiteRecordList &GetPerSiteRecords(const std::string &site) {
		return per_site_records[site];
	}

	const Record &Emplace(ConstBuffer<uint8_t> raw);
};
