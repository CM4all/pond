/*
 * Copyright 2017-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "RList.hxx"
#include "system/LargeAllocation.hxx"

#include <unordered_map>
#include <vector>
#include <string>

template<typename T> struct ConstBuffer;
struct Filter;
class Selection;
class AppendListener;
class AnyRecordList;

class Database {
	const LargeAllocation allocation;

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
	~Database() noexcept;

	Database(const Database &) = delete;
	Database &operator=(const Database &) = delete;

	void Clear() noexcept {
		for (auto &i : per_site_records)
			// TODO: purge unreferenced lists
			i.second.clear();

		all_records.clear();

		// TODO: madvise(MADV_DONTNEED)
	}

	FullRecordList &GetAllRecords() noexcept {
		return all_records;
	}

	PerSiteRecordList &GetPerSiteRecords(const std::string &site) noexcept {
		return per_site_records[site];
	}

	const Record &Emplace(ConstBuffer<uint8_t> raw) noexcept;

	Selection Select(const Filter &filter) noexcept;
	Selection Follow(const Filter &filter, AppendListener &l) noexcept;

	/**
	 * Collect a list of site names matching the given filter.
	 * They appear in the returned array in the order of
	 * appearance.
	 */
	std::vector<std::string> CollectSites(const Filter &filter) noexcept;

private:
	AnyRecordList GetList(Filter &filter) noexcept;
	Selection MakeSelection(const Filter &filter) noexcept;
};
