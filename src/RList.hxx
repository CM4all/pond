/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Record.hxx"
#include "AppendListener.hxx"
#include "util/VCircularBuffer.hxx"

#include <boost/intrusive/list.hpp>

#include <assert.h>

template<Record::ListHook Record::*list_hook>
class RecordList {
	typedef boost::intrusive::list<Record,
				       boost::intrusive::member_hook<Record,
								     Record::ListHook,
								     list_hook>,
				       boost::intrusive::constant_time_size<false>> List;

	List list;

	AppendListenerList append_listeners;

public:
	RecordList() = default;

	~RecordList() {
		assert(list.empty());
		assert(append_listeners.empty());
	}

	RecordList(const RecordList &) = delete;
	RecordList &operator=(const RecordList &) = delete;

	typedef typename List::const_iterator const_iterator;

	const_iterator begin() const {
		return list.begin();
	}

	const_iterator end() const {
		return list.end();
	}

	Record &front() {
		return list.front();
	}

	void clear() {
		list.clear();
	}

	template<typename Disposer>
	void clear_and_dispose(Disposer &&d) {
		list.clear_and_dispose(std::forward<Disposer>(d));
	}

	void push_back(Record &record) {
		list.push_back(record);

		append_listeners.clear_and_dispose([&record](AppendListener *l){
				l->OnAppend(record);
			});
	}

	const Record *First() const {
		return list.empty() ? nullptr : &list.front();
	}

	const Record *Next(const Record &current) const {
		auto i = list.iterator_to(current);
		++i;
		return i == list.end()
			? nullptr
			: &*i;
	}

	void AddAppendListener(AppendListener &l) {
		append_listeners.push_back(l);
	}
};

class FullRecordList : public VCircularBuffer<Record> {
	AppendListenerList append_listeners;

public:
	using VCircularBuffer::VCircularBuffer;

	~FullRecordList() noexcept {
		assert(append_listeners.empty());
	}

	template<typename... Args>
	reference emplace_back(Args... args) {
		auto &record =
			VCircularBuffer::emplace_back(std::forward<Args>(args)...);

		append_listeners.clear_and_dispose([&record](AppendListener *l){
				l->OnAppend(record);
			});

		return record;
	}

	const Record *First() const {
		return empty() ? nullptr : &front();
	}

	const Record *Next(const Record &current) const {
		auto i = iterator_to(current);
		++i;
		return i == end()
			? nullptr
			: &*i;
	}

	void AddAppendListener(AppendListener &l) {
		append_listeners.push_back(l);
	}
};

class PerSiteRecordList : public RecordList<&Record::per_site_list_hook> {};
