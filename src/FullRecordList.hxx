// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

	void Compress() noexcept {
		skip_deque.Compress();
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

	const Record *Previous(const Record &current) const noexcept {
		auto i = iterator_to(current);
		if (i == begin())
			return nullptr;

		--i;
		return &*i;
	}

	[[gnu::pure]]
	const Record *TimeLowerBound(Net::Log::TimePoint since) noexcept {
		if (empty())
			return nullptr;

		skip_deque.FixDeleted(front());
		return skip_deque.TimeLowerBound(since);
	}

	[[gnu::pure]]
	const Record *LastUntil(Net::Log::TimePoint until) noexcept {
		if (empty())
			return nullptr;

		skip_deque.FixDeleted(front());
		return skip_deque.LastUntil(until);
	}

	void AddAppendListener(AppendListener &l) noexcept {
		append_listeners.Add(l);
	}
};
