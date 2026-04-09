// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Record.hxx"
#include "RSkipDeque.hxx"
#include "AppendListener.hxx"
#include "util/VCircularBuffer.hxx"

#include <assert.h>

class FullRecordList {
	using List = VCircularBuffer<Record>;
	List list;

	RecordSkipDeque skip_deque;

	AppendListenerList append_listeners;

public:
	explicit FullRecordList(std::span<std::byte> buffer) noexcept
		:list(buffer) {}

	~FullRecordList() noexcept {
		assert(append_listeners.empty());
	}

	[[gnu::pure]]
	auto GetMemoryUsage() const noexcept {
		return list.GetMemoryUsage();
	}

	void Compress() noexcept {
		if (list.empty()) {
			skip_deque.clear();
		} else {
			FixDeleted();
			skip_deque.Compress();
		}
	}

	bool empty() const noexcept {
		return list.empty();
	}

	auto size() const noexcept {
		return list.size();
	}

	void clear() noexcept {
		list.clear();
	}

	const auto &front() const noexcept {
		return list.front();
	}

	const auto &back() const noexcept {
		return list.back();
	}

	void pop_front() noexcept {
		list.pop_front();
	}

	template<typename... Args>
	List::reference emplace_back(Args... args) {
		auto &record =
			list.emplace_back(std::forward<Args>(args)...);
		skip_deque.UpdateNew(record);

		append_listeners.OnAppend(record);

		return record;
	}

	template<typename C, typename... Args>
	List::reference check_emplace_back(C &&check, Args... args) {
		auto &record =
			list.check_emplace_back(std::forward<C>(check),
						std::forward<Args>(args)...);
		skip_deque.UpdateNew(record);

		append_listeners.OnAppend(record);

		return record;
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

	const Record *Previous(const Record &current) const noexcept {
		auto i = list.iterator_to(current);
		if (i == list.begin())
			return nullptr;

		--i;
		return &*i;
	}

	[[gnu::pure]]
	const Record *TimeLowerBound(Net::Log::TimePoint since) noexcept {
		if (list.empty())
			return nullptr;

		FixDeleted();
		return skip_deque.TimeLowerBound(since);
	}

	[[gnu::pure]]
	const Record *LastUntil(Net::Log::TimePoint until) noexcept {
		if (list.empty())
			return nullptr;

		FixDeleted();
		return skip_deque.LastUntil(until);
	}

	void AddAppendListener(AppendListener &l) noexcept {
		append_listeners.Add(l);
	}

private:
	void FixDeleted() noexcept {
		assert(!list.empty());

		skip_deque.FixDeleted(list.front());
	}
};
