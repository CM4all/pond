// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Record.hxx"
#include "RSkipDeque.hxx"
#include "AppendListener.hxx"
#include "util/IntrusiveList.hxx"

#include <cassert>

template<Record::ListHook Record::*list_hook>
class RecordList {
	using List = IntrusiveList<Record,
				   IntrusiveListMemberHookTraits<list_hook>>;

	List list;

	RecordSkipDeque skip_deque;

	AppendListenerList append_listeners;

public:
	RecordList() = default;

	~RecordList() noexcept {
		assert(append_listeners.empty());
	}

	RecordList(const RecordList &) = delete;
	RecordList &operator=(const RecordList &) = delete;

	void Compress() noexcept {
		skip_deque.Compress();
	}

	bool IsExpendable() const noexcept {
		return list.empty() && append_listeners.empty();
	}

	using const_iterator = typename List::const_iterator;

	const_iterator begin() const noexcept {
		return list.begin();
	}

	const_iterator end() const noexcept {
		return list.end();
	}

	Record &front() noexcept {
		return list.front();
	}

	void clear() noexcept {
		list.clear();
	}

	void push_back(Record &record) noexcept {
		list.push_back(record);
		skip_deque.UpdateNew(record);

		append_listeners.OnAppend(record);
	}

	const Record *First() const noexcept {
		return list.empty() ? nullptr : &list.front();
	}

	const Record *Last() const noexcept {
		return list.empty() ? nullptr : &list.back();
	}

	const Record *Next(const Record &current) const noexcept {
		auto i = list.iterator_to(current);
		++i;
		return i == list.end()
			? nullptr
			: &*i;
	}

	[[gnu::pure]]
	const Record *TimeLowerBound(Net::Log::TimePoint since) noexcept {
		if (list.empty())
			return nullptr;

		skip_deque.FixDeleted(front());
		return skip_deque.TimeLowerBound(since);
	}

	void AddAppendListener(AppendListener &l) noexcept {
		append_listeners.Add(l);
	}
};

class PerSiteRecordList : public RecordList<&Record::per_site_list_hook> {};
