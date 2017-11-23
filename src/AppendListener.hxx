/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <boost/intrusive/list.hpp>

/**
 * An iterator for records in the #Database.  While an instance
 * exists, the database may be modified, because FixDeleted() will
 * take care for cleaning up invalid pointers.
 */
class AppendListener
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>> {
public:
	/**
	 * Callback invoked by the #Database.
	 *
	 * @return false to remove the listener from the
	 * #AppendListenerList
	 */
	virtual bool OnAppend(const Record &record) noexcept = 0;
};

class AppendListenerList {
	boost::intrusive::list<AppendListener,
			       boost::intrusive::constant_time_size<false>> list;

public:
	bool empty() const noexcept {
		return list.empty();
	}

	void Add(AppendListener &l) {
		list.push_back(l);
	}

	void OnAppend(const Record &record) noexcept {
		list.remove_if([&record](const AppendListener &_l){
				auto &l = const_cast<AppendListener &>(_l);
				return l.OnAppend(record);
			});
	}
};
