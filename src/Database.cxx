// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Database.hxx"
#include "Selection.hxx"
#include "Filter.hxx"
#include "AnyList.hxx"
#include "system/HugePage.hxx"
#include "system/PageAllocator.hxx"
#include "system/VmaName.hxx"
#include "time/Cast.hxx"
#include "time/ClockCache.hxx"
#include "util/DeleteDisposer.hxx"

#include <assert.h>

static std::string_view
NullableStringView(const char *s) noexcept
{
	return s != nullptr ? std::string_view{s} : std::string_view{};
}

void
Database::PerSite::OnAbandoned() noexcept
{
	if (list.IsExpendable())
		delete this;
}

Database::Database(size_t max_size, double _per_site_message_rate_limit)
	:allocation(AlignHugePageUp(max_size)),
	 per_site_message_rate_limit{
		.rate = _per_site_message_rate_limit,
		.burst = 10 * _per_site_message_rate_limit, // TODO: make burst configurable
	 },
	 all_records({(std::byte *)allocation.get(), allocation.size()})
{
	EnableHugePages(allocation.get(), allocation.size());
	EnablePageFork(allocation.get(), allocation.size(), false);

	if (max_size > 2ull * 1024 * 1024 * 1024)
		/* exclude database memory from core dumps if it's
		   extremely large, because such a large memory
		   section usually doesn't fit in the core dump
		   partition, which would effectively make core dumps
		   impossible */
		EnablePageDump(allocation.get(), allocation.size(), false);

	SetVmaName(allocation.get(), allocation.size(), "PondDatabase");
}

Database::~Database() noexcept
{
	per_site_records.clear_and_dispose(DeleteDisposer{});
	all_records.clear();
}

void
Database::Clear() noexcept
{
	for (auto i = site_list.begin(); i != site_list.end();) {
		i->list.clear();

		if (i->IsExpendable())
			i = site_list.erase_and_dispose(i, DeleteDisposer{});
		else
			++i;
	}

	all_records.clear();

	// TODO: madvise(MADV_DONTNEED)
}

void
Database::Compress() noexcept
{
	all_records.Compress();

	for (auto i = site_list.begin(); i != site_list.end();) {
		i->Compress();

		if (i->IsExpendable())
			i = site_list.erase_and_dispose(i, DeleteDisposer{});
		else
			++i;
	}
}

const Record &
Database::Emplace(std::span<const std::byte> raw)
{
	auto &record = all_records.emplace_back(sizeof(Record) + raw.size(),
						++last_id, raw);

	GetPerSiteRecords(NullableStringView(record.GetParsed().site)).push_back(record);

	return record;
}

namespace {
struct RateLimitExceeded {};
}

static constexpr bool
IsMessage(const Net::Log::Type type) noexcept
{
	return type == Net::Log::Type::HTTP_ERROR;
}

static constexpr bool
IsMessage(const SmallDatagram &d) noexcept
{
	return IsMessage(d.type);
}

const Record *
Database::CheckEmplace(std::span<const std::byte> raw,
		       const ClockCache<std::chrono::steady_clock> &clock)
try {
	if (per_site_message_rate_limit.rate <= 0)
		/* no rate limit configured */
		return &Emplace(raw);

	auto &record = all_records.check_emplace_back([this, &clock](const Record &r){
		if (!IsMessage(r.GetParsed()))
			/* not a message, not affected by the rate
			   limit */
			return;

		const char *site = r.GetParsed().site;
		if (site == nullptr)
			return;

		const auto now = clock.now();
		const auto float_now = ToFloatSeconds(now.time_since_epoch());

		auto &per_site = GetPerSite(site);
		if (!per_site.CheckRateLimit(per_site_message_rate_limit, float_now, 1))
			throw RateLimitExceeded();
	}, sizeof(Record) + raw.size(), ++last_id, raw);

	GetPerSiteRecords(NullableStringView(record.GetParsed().site)).push_back(record);

	return &record;
} catch (RateLimitExceeded) {
	return nullptr;
}

Database::PerSite &
Database::GetPerSite(std::string_view site) noexcept
{
	auto [it, inserted] =
		per_site_records.insert_check(site);
	if (inserted) {
		auto *per_site = new PerSite(site);
		it = per_site_records.insert_commit(it, *per_site);
		site_list.push_back(*per_site);
	}

	return *it;
}

std::pair<AnyRecordList, SharedLease>
Database::GetList(Filter &filter) noexcept
{
	if (filter.HasOneSite()) {
		auto &per_site = GetPerSite(*filter.sites.begin());

		/* the PerSiteRecordList is already filtered for site;
		   we can disable it in the Filter, because that check
		   would be redundant */
		filter.sites.clear();

		return {per_site.list, per_site};
	} else
		return {GetAllRecords(), {}};
}

inline Selection
Database::MakeSelection(const Filter &_filter) noexcept
{
	Filter filter(_filter);
	auto [list, lease] = GetList(filter);
	return Selection(list, filter, std::move(lease));
}

Selection
Database::Select(const Filter &filter) noexcept
{
	auto selection = MakeSelection(filter);
	selection.Rewind();
	return selection;
}

Selection
Database::Follow(const Filter &filter, AppendListener &l) noexcept
{
	auto selection = MakeSelection(filter);
	selection.AddAppendListener(l);
	return selection;
}

Selection
Database::Select(const SiteIterator &_site, const Filter &filter) noexcept
{
	assert(_site);
	assert(filter.sites.empty());

	auto &site = static_cast<PerSite &>(_site.lease.GetAnchor());
	Selection selection(site.list, filter, _site.lease);
	selection.Rewind();
	return selection;
}
