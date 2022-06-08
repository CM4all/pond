/*
 * Copyright 2017-2022 CM4all GmbH
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

#include "Record.hxx"
#include "RSkipDeque.hxx"
#include "AppendListener.hxx"
#include "util/VCircularBuffer.hxx"

#include <assert.h>

class FullRecordList : public VCircularBuffer<Record> {
	RecordSkipDeque skip_deque;

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
		skip_deque.UpdateNew(record);

		append_listeners.OnAppend(record);

		return record;
	}

	template<typename C, typename... Args>
	reference check_emplace_back(C &&check, Args... args) {
		auto &record =
			VCircularBuffer::check_emplace_back(std::forward<C>(check),
							    std::forward<Args>(args)...);
		skip_deque.UpdateNew(record);

		append_listeners.OnAppend(record);

		return record;
	}

	const Record *First() const noexcept {
		return empty() ? nullptr : &front();
	}

	const Record *Last() const noexcept {
		return empty() ? nullptr : &back();
	}

	const Record *Next(const Record &current) const noexcept {
		auto i = iterator_to(current);
		++i;
		return i == end()
			? nullptr
			: &*i;
	}

	gcc_pure
	const Record *TimeLowerBound(Net::Log::TimePoint since) noexcept {
		if (empty())
			return nullptr;

		skip_deque.FixDeleted(front());
		return skip_deque.TimeLowerBound(since);
	}

	void AddAppendListener(AppendListener &l) noexcept {
		append_listeners.Add(l);
	}
};
