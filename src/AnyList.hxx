// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/log/Chrono.hxx"

#include <utility>

#include <stdint.h>

class Record;
class FullRecordList;
class PerSiteRecordList;
class AppendListener;

/**
 * A wrapper which accesses either a #FullRecordList or a
 * #PerSiteRecordList, depending on the filter.
 */
class AnyRecordList {
	FullRecordList *all = nullptr;
	PerSiteRecordList *per_site = nullptr;

public:
	constexpr AnyRecordList() noexcept = default;
	constexpr AnyRecordList(FullRecordList &list) noexcept:all(&list) {}
	constexpr AnyRecordList(PerSiteRecordList &list) noexcept:per_site(&list) {}

	[[gnu::pure]]
	const Record *TimeLowerBound(Net::Log::TimePoint since) const noexcept;

	[[gnu::pure]]
	const Record *First() const noexcept;

	[[gnu::pure]]
	const Record *Last() const noexcept;

	[[gnu::pure]]
	const Record *Next(const Record &r) const noexcept;

	[[gnu::pure]]
	const Record *Previous(const Record &r) const noexcept;

	void AddAppendListener(AppendListener &l) const noexcept;
};
