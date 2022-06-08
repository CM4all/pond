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

#include "net/log/Chrono.hxx"
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
	const Record *TimeLowerBound(Net::Log::TimePoint since) const noexcept;

	gcc_pure
	const Record *First() const noexcept;

	gcc_pure
	const Record *Next(const Record &r) const noexcept;

	void AddAppendListener(AppendListener &l) const noexcept;
};
