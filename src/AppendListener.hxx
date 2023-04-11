// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/IntrusiveList.hxx"

class Record;

/**
 * An iterator for records in the #Database.  While an instance
 * exists, the database may be modified, because FixDeleted() will
 * take care for cleaning up invalid pointers.
 */
class AppendListener {
	friend class AppendListenerList;
	IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK> siblings;

public:
	bool IsRegistered() const noexcept {
		return siblings.is_linked();
	}

	void Unregister() noexcept {
		siblings.unlink();
	}

	/**
	 * Callback invoked by the #Database.
	 *
	 * @return false to remove the listener from the
	 * #AppendListenerList
	 */
	virtual bool OnAppend(const Record &record) noexcept = 0;
};

class AppendListenerList {
	IntrusiveList<AppendListener,
		      IntrusiveListMemberHookTraits<&AppendListener::siblings>> list;

public:
	bool empty() const noexcept {
		return list.empty();
	}

	void Add(AppendListener &l) {
		list.push_back(l);
	}

	void OnAppend(const Record &record) noexcept {
		list.remove_and_dispose_if([&record](const AppendListener &_l){
			auto &l = const_cast<AppendListener &>(_l);
			return !l.OnAppend(record);
		}, [](auto *){});
	}
};
