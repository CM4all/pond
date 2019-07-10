/*
 * Copyright 2017-2019 Content Management AG
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

#pragma once

#include <boost/intrusive/list.hpp>

class Record;

/**
 * An iterator for records in the #Database.  While an instance
 * exists, the database may be modified, because FixDeleted() will
 * take care for cleaning up invalid pointers.
 */
class AppendListener {
	friend class AppendListenerList;
	typedef boost::intrusive::list_member_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>> SiblingsHook;
	SiblingsHook siblings;

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
	boost::intrusive::list<AppendListener,
			       boost::intrusive::member_hook<AppendListener,
							     AppendListener::SiblingsHook,
							     &AppendListener::siblings>,
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
				return !l.OnAppend(record);
			});
	}
};
