/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "util/Compiler.h"

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

	gcc_pure
	std::pair<const Record *,
		  const Record *> TimeRange(uint64_t since,
					    uint64_t until) const noexcept;

	gcc_pure
	const Record *First() const noexcept;

	gcc_pure
	const Record *Next(const Record &r) const;

	void AddAppendListener(AppendListener &l) const noexcept;
};
