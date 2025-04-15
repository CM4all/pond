// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "FullRecordList.hxx"
#include "RList.hxx"
#include "system/LargeAllocation.hxx"
#include "util/TokenBucket.hxx"
#include "util/IntrusiveForwardList.hxx"

#include <unordered_map>
#include <span>
#include <string>

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

	const TokenBucketConfig per_site_message_rate_limit;

	uint64_t last_id = 0;

	/**
	 * A chronological list of all records.  This list "owns" the
	 * allocated #Record instances.
	 */
	FullRecordList all_records;

	struct PerSite final : SiteIterator {
		IntrusiveForwardListHook list_siblings;

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

		bool CheckRateLimit(const TokenBucketConfig config,
				    double now, double size) noexcept {
			return rate_limiter.Check(config, now, size);
		}
	};

	// TODO: purge empty items eventually
	std::unordered_map<std::string, PerSite> per_site_records;

	/**
	 * A linked list of all sites; this can be used to iterate
	 * incrementally over all known sites.
	 */
	IntrusiveForwardList<
		PerSite,
		IntrusiveForwardListMemberHookTraits<&PerSite::list_siblings>,
		IntrusiveForwardListOptions{.constant_time_size = true, .cache_last = true}> site_list;

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
	const Record &Emplace(std::span<const std::byte> raw);

	/**
	 * Throws if parsing the buffer fails.
	 *
	 * @return a pointer to the new record or nullptr if a rate
	 * limit was exceeded
	 */
	const Record *CheckEmplace(std::span<const std::byte> raw,
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
