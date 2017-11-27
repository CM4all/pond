/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Database.hxx"
#include "Selection.hxx"
#include "Filter.hxx"
#include "AnyList.hxx"
#include "system/HugePage.hxx"

#include <assert.h>
#include <sys/mman.h>

Database::Database(size_t max_size)
	:allocation(AlignHugePageUp(max_size)),
	 all_records({allocation.get(), allocation.size()})
{
	madvise(allocation.get(), allocation.size(), MADV_HUGEPAGE);
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
Database::GetList(const Filter &filter) noexcept
{
	return filter.site.empty()
		? AnyRecordList(GetAllRecords())
		: AnyRecordList(GetPerSiteRecords(filter.site));
}

Selection
Database::Select(const Filter &filter) noexcept
{
	Selection selection(GetList(filter), filter);
	selection.Rewind();
	return selection;
}

Selection
Database::Follow(const Filter &filter, AppendListener &l) noexcept
{
	Selection selection(GetList(filter), filter);
	selection.AddAppendListener(l);
	return selection;
}
