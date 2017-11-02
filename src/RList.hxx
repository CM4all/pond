/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Record.hxx"
#include "Cursor.hxx"

#include <boost/intrusive/list.hpp>

#include <assert.h>

template<Record::ListHook Record::*list_hook>
class RecordList {
	typedef boost::intrusive::list<Record,
				       boost::intrusive::member_hook<Record,
								     Record::ListHook,
								     list_hook>,
				       boost::intrusive::constant_time_size<true>> List;

	List list;

	CursorList follow_cursors;

public:
	RecordList() = default;

	~RecordList() {
		assert(list.empty());
		assert(follow_cursors.empty());
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

	size_t size() const {
		return list.size();
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

		follow_cursors.clear_and_dispose([&record](Cursor *cursor){
				cursor->OnAppend(record);
			});
	}

	void remove(Record &record) {
		list.erase(list.iterator_to(record));
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

	void Follow(Cursor &cursor) {
		follow_cursors.push_back(cursor);
	}
};

class FullRecordList : public RecordList<&Record::list_hook> {};
class PerSiteRecordList : public RecordList<&Record::per_site_list_hook> {};
