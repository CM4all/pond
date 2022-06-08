/*
 * Copyright 2017-2022 CM4all GmbH
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

#include "FullRecordList.hxx"
#include "RList.hxx"
#include "system/LargeAllocation.hxx"
#include "util/TokenBucket.hxx"

#include <boost/intrusive/slist.hpp>

#include <unordered_map>
#include <string>

template<typename T> struct ConstBuffer;
template<typename Clock> class ClockCache;
struct Filter;
class Selection;
class AppendListener;
class AnyRecordList;

/**
 * This class is used by Database::GetFirstSite() and
 * Database::GetNextSite() to iterate over all sites.
 */
class SiteIterator {
protected:
	SiteIterator() noexcept = default;
	~SiteIterator() noexcept = default;
	SiteIterator(const SiteIterator &) = delete;
	SiteIterator &operator=(const SiteIterator &) = delete;
};

class Database {
	const LargeAllocation allocation;

	const double per_site_message_rate_limit;
	const double per_site_message_burst;

	uint64_t last_id = 0;

	/**
	 * A chronological list of all records.  This list "owns" the
	 * allocated #Record instances.
	 */
	FullRecordList all_records;

	struct PerSite final : SiteIterator {
		using ListHook =
			boost::intrusive::slist_member_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>;
		ListHook list_siblings;

		/**
		 * A chronological list for each site.  This list does not
		 * "own" the #Record instances, it only points to those owned
		 * by #all_records.
		 */
		PerSiteRecordList list;

		TokenBucket rate_limiter;

		~PerSite() noexcept {
			list.clear();
		}

		bool CheckRateLimit(double now, double rate, double burst,
				    size_t size) noexcept {
			return rate_limiter.Check(now, rate, burst, size);
		}
	};

	// TODO: purge empty items eventually
	std::unordered_map<std::string, PerSite> per_site_records;

	using SiteList =
		boost::intrusive::slist<PerSite,
					boost::intrusive::member_hook<PerSite,
								      typename PerSite::ListHook,
								      &PerSite::list_siblings>,
					boost::intrusive::cache_last<true>,
					boost::intrusive::constant_time_size<true>>;

	/**
	 * A linked list of all sites; this can be used to iterate
	 * incrementally over all known sites.
	 */
	SiteList site_list;

public:
	explicit Database(size_t max_size, double _per_site_message_rate_limit=-1);
	~Database() noexcept;

	Database(const Database &) = delete;
	Database &operator=(const Database &) = delete;

	auto GetMemoryCapacity() const noexcept {
		return allocation.size();
	}

	auto GetMemoryUsage() const noexcept {
		return all_records.GetMemoryUsage();
	}

	auto GetRecordCount() const noexcept {
		return all_records.size();
	}

	void Clear() noexcept {
		site_list.clear();
		per_site_records.clear();

		all_records.clear();

		// TODO: madvise(MADV_DONTNEED)
	}

	void DeleteOlderThan(Net::Log::TimePoint t) noexcept {
		while (!all_records.empty() &&
		       all_records.front().IsOlderThanOrUnknown(t))
			all_records.pop_front();
	}

	FullRecordList &GetAllRecords() noexcept {
		return all_records;
	}

	/**
	 * Throws if parsing the buffer fails.
	 */
	const Record &Emplace(ConstBuffer<std::byte> raw);

	/**
	 * Throws if parsing the buffer fails.
	 *
	 * @return a pointer to the new record or nullptr if a rate
	 * limit was exceeded
	 */
	const Record *CheckEmplace(ConstBuffer<std::byte> raw,
				   const ClockCache<std::chrono::steady_clock> &clock);

	Selection Select(const Filter &filter) noexcept;
	Selection Follow(const Filter &filter, AppendListener &l) noexcept;

	SiteIterator *GetFirstSite(unsigned skip=0) noexcept {
		for (auto i = site_list.begin(); i != site_list.end(); ++i)
			if (skip-- == 0)
				return &*i;

		return nullptr;
	}

	SiteIterator *GetNextSite(SiteIterator &_previous) noexcept {
		auto &previous = (PerSite &)_previous;
		auto i = site_list.iterator_to(previous);
		++i;
		if (i == site_list.end())
			return nullptr;
		return &*i;
	}

	Selection Select(SiteIterator &site, const Filter &filter) noexcept;

private:
	template<typename S>
	auto &GetPerSite(S &&site) noexcept {
		auto e = per_site_records.emplace(std::piecewise_construct,
						  std::forward_as_tuple(std::forward<S>(site)),
						  std::forward_as_tuple());
		auto &per_site = e.first->second;

		if (e.second)
			site_list.push_back(per_site);

		return per_site;
	}

	template<typename S>
	auto &GetPerSiteRecords(S &&site) noexcept {
		return GetPerSite(std::forward<S>(site)).list;
	}

	AnyRecordList GetList(Filter &filter) noexcept;
	Selection MakeSelection(const Filter &filter) noexcept;
};
