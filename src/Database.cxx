/*
 * author: Max Kellermann <mk@cm4all.com>
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

Database::~Database()
{
	for (auto &i : per_site_records)
		i.second.clear();

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
