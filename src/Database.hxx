// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "FullRecordList.hxx"
#include "RList.hxx"
#include "SiteIterator.hxx"
#include "system/LargeAllocation.hxx"
#include "util/TokenBucket.hxx"
#include "util/IntrusiveList.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/SharedLease.hxx"

#include <cassert>
#include <span>
#include <string>

template<typename Clock> class ClockCache;
struct Filter;
class Selection;
class AppendListener;
class AnyRecordList;

class Database {
	const LargeAllocation allocation;

	const TokenBucketConfig per_site_message_rate_limit;

	uint64_t last_id = 0;

	/**
	 * A chronological list of all records.  This list "owns" the
	 * allocated #Record instances.
	 */
	FullRecordList all_records;

	struct PerSite final
		: IntrusiveHashSetHook<IntrusiveHookMode::AUTO_UNLINK>,
		  IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK>,
		  SharedAnchor
	{
		const std::string site;

		/**
		 * A chronological list for each site.  This list does not
		 * "own" the #Record instances, it only points to those owned
		 * by #all_records.
		 */
		PerSiteRecordList list;

		TokenBucket rate_limiter;

		explicit PerSite(std::string_view _site) noexcept
			:site(_site) {}

		bool IsExpendable() const noexcept {
			return list.IsExpendable() && IsAbandoned();
		}

		void Compress() noexcept {
			list.Compress();
		}

		bool CheckRateLimit(const TokenBucketConfig config,
				    double now, double size) noexcept {
			return rate_limiter.Check(config, now, size);
		}

		// virtual methods from SharedAnchor
		void OnAbandoned() noexcept override;

		struct GetSite {
			constexpr std::string_view operator()(const PerSite &per_site) const noexcept {
				return per_site.site;
			}
		};
	};

	IntrusiveHashSet<
		PerSite, 65536,
		IntrusiveHashSetOperators<PerSite, PerSite::GetSite,
					  std::hash<std::string_view>,
					  std::equal_to<std::string_view>>> per_site_records;

	/**
	 * A linked list of all sites; this can be used to iterate
	 * incrementally over all known sites.
	 */
	IntrusiveList<PerSite> site_list;

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

	void Clear() noexcept;

	/**
	 * Shrink data structures to fit the actual size.
	 */
	void Compress() noexcept;

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

	[[gnu::pure]]
	Selection Select(const Filter &filter) noexcept;

	[[gnu::pure]]
	Selection Follow(const Filter &filter, AppendListener &l) noexcept;

	[[gnu::pure]]
	SiteIterator GetFirstSite(unsigned skip=0) noexcept {
		for (auto &i : site_list)
			if (skip-- == 0)
				return {i};

		return {};
	}

	[[gnu::pure]]
	SiteIterator GetNextSite(const SiteIterator &_previous) noexcept {
		assert(_previous);
		auto &previous = static_cast<PerSite &>(_previous.lease.GetAnchor());
		const auto i = std::next(site_list.iterator_to(previous));
		if (i == site_list.end())
			return {};
		return {*i};
	}

	[[gnu::pure]]
	Selection Select(const SiteIterator &site, const Filter &filter) noexcept;

private:
	[[gnu::pure]]
	PerSite &GetPerSite(std::string_view site) noexcept;

	auto &GetPerSiteRecords(std::string_view site) noexcept {
		return GetPerSite(site).list;
	}

	std::pair<AnyRecordList, SharedLease> GetList(Filter &filter) noexcept;
	Selection MakeSelection(const Filter &filter) noexcept;
};
