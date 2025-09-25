// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "AnyList.hxx"
#include "RList.hxx"
#include "FullRecordList.hxx"

const Record *
AnyRecordList::TimeLowerBound(Net::Log::TimePoint since) const noexcept
{
	return all != nullptr
		? all->TimeLowerBound(since)
		: (per_site != nullptr
		   ? per_site->TimeLowerBound(since)
		   : nullptr);
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
AnyRecordList::Last() const noexcept
{
	return all != nullptr
		? all->Last()
		: (per_site != nullptr
		   ? per_site->Last()
		   : nullptr);
}

const Record *
AnyRecordList::Next(const Record &r) const noexcept
{
	return all != nullptr
		? all->Next(r)
		: per_site->Next(r);
}

const Record *
AnyRecordList::Previous(const Record &r) const noexcept
{
	return all != nullptr
		? all->Previous(r)
		: per_site->Previous(r);
}

void
AnyRecordList::AddAppendListener(AppendListener &l) const noexcept
{
	if (all != nullptr)
		all->AddAppendListener(l);
	else if (per_site != nullptr)
		per_site->AddAppendListener(l);
}
