/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "AnyList.hxx"
#include "RList.hxx"

std::pair<const Record *, const Record *>
AnyRecordList::TimeRange(uint64_t since,
			 uint64_t until) const noexcept
{
	return all != nullptr
		? all->TimeRange(since, until)
		: (per_site != nullptr
		   ? per_site->TimeRange(since, until)
		   : std::make_pair(nullptr, nullptr));
}

const Record *
AnyRecordList::First() const noexcept
{
	return all != nullptr
		? all->First()
		: (per_site != nullptr
		   ? per_site->First()
		   : nullptr);
}

const Record *
AnyRecordList::Next(const Record &r) const
{
	return all != nullptr
		? all->Next(r)
		: per_site->Next(r);
}

void
AnyRecordList::AddAppendListener(AppendListener &l) const noexcept
{
	if (all != nullptr)
		all->AddAppendListener(l);
	else if (per_site != nullptr)
		per_site->AddAppendListener(l);
}
