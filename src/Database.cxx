/*
 * Copyright 2017-2019 Content Management AG
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

#include "Database.hxx"
#include "Selection.hxx"
#include "Filter.hxx"
#include "AnyList.hxx"
#include "system/HugePage.hxx"
#include "time/Cast.hxx"
#include "time/ClockCache.hxx"

#include <assert.h>
#include <sys/mman.h>

Database::Database(size_t max_size, double _per_site_message_rate_limit)
	:allocation(AlignHugePageUp(max_size)),
	 per_site_message_rate_limit(_per_site_message_rate_limit),
	 per_site_message_burst(10 * per_site_message_rate_limit), // TODO: make burst configurable
	 all_records({allocation.get(), allocation.size()})
{
	madvise(allocation.get(), allocation.size(), MADV_DONTFORK);
	madvise(allocation.get(), allocation.size(), MADV_HUGEPAGE);

	if (max_size > 2ull * 1024 * 1024 * 1024)
		/* exclude database memory from core dumps if it's
		   extremely large, because such a large memory
		   section usually doesn't fit in the core dump
		   partition, which would effectively make core dumps
		   impossible */
		madvise(allocation.get(), allocation.size(), MADV_DONTDUMP);
}

Database::~Database() noexcept
{
	all_records.clear();
}

const Record &
Database::Emplace(ConstBuffer<uint8_t> raw)
{
	auto &record = all_records.emplace_back(sizeof(Record) + raw.size,
						++last_id, raw);

	if (record.GetParsed().site != nullptr)
		GetPerSiteRecords(record.GetParsed().site).push_back(record);

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
Database::CheckEmplace(ConstBuffer<uint8_t> raw,
		       const ClockCache<std::chrono::steady_clock> &clock)
try {
	if (per_site_message_rate_limit <= 0)
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
		if (!per_site.CheckRateLimit(float_now, per_site_message_rate_limit,
					     per_site_message_burst, 1))
			throw RateLimitExceeded();
	}, sizeof(Record) + raw.size, ++last_id, raw);

	if (record.GetParsed().site != nullptr)
		GetPerSiteRecords(record.GetParsed().site).push_back(record);

	return &record;
} catch (RateLimitExceeded) {
	return nullptr;
}

AnyRecordList
Database::GetList(Filter &filter) noexcept
{
	if (filter.HasOneSite()) {
		auto &list = GetPerSiteRecords(*filter.sites.begin());

		/* the PerSiteRecordList is already filtered for site;
		   we can disable it in the Filter, because that check
		   would be redundant */
		filter.sites.clear();

		return list;
	} else
		return GetAllRecords();
}

inline Selection
Database::MakeSelection(const Filter &_filter) noexcept
{
	Filter filter(_filter);
	auto list = GetList(filter);
	return Selection(list, filter);
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
Database::Select(SiteIterator &_site, const Filter &filter) noexcept
{
	assert(filter.sites.empty());

	auto &site = (PerSite &)_site;
	Selection selection(site.list, filter);
	selection.Rewind();
	return selection;
}
