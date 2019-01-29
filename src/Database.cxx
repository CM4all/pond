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

#include "Database.hxx"
#include "Selection.hxx"
#include "Filter.hxx"
#include "AnyList.hxx"
#include "system/HugePage.hxx"

#include <unordered_set>

#include <assert.h>
#include <sys/mman.h>

Database::Database(size_t max_size)
	:allocation(AlignHugePageUp(max_size)),
	 all_records({allocation.get(), allocation.size()})
{
	int advice = MADV_DONTFORK|MADV_HUGEPAGE;

	if (max_size > 2ull * 1024 * 1024 * 1024)
		/* exclude database memory from core dumps if it's
		   extremely large, because such a large memory
		   section usually doesn't fit in the core dump
		   partition, which would effectively make core dumps
		   impossible */
		advice |= MADV_DONTDUMP;

	madvise(allocation.get(), allocation.size(), advice);
}

Database::~Database() noexcept
{
	for (auto &i : per_site_records)
		i.second.clear();

	all_records.clear();
}

const Record &
Database::Emplace(ConstBuffer<uint8_t> raw) noexcept
{
	auto &record = all_records.emplace_back(sizeof(Record) + raw.size,
						++last_id, raw);

	if (record.GetParsed().site != nullptr)
		GetPerSiteRecords(record.GetParsed().site).push_back(record);

	return record;
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

std::vector<std::string>
Database::CollectSites(const Filter &filter) noexcept
{
	std::unordered_set<std::string> s;
	std::vector<std::string> v;

	for (auto i = Select(filter); i; ++i) {
		const char *site = i->GetParsed().site;
		if (site == nullptr)
			continue;

		auto e = s.emplace(site);
		if (!e.second)
			continue;

		v.emplace_back(*e.first);
	}

	return v;
}
