/*
 * Copyright 2017-2018 Content Management AG
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

#include "AnyList.hxx"

#include <cstddef>
#include <utility>

#include <assert.h>
#include <stdint.h>

class Record;

/**
 * An iterator for records in the #Database.  While an instance
 * exists, the database must not be modified.
 */
class LightCursor {
	AnyRecordList list;

	const Record *next = nullptr;

public:
	explicit LightCursor(std::nullptr_t) noexcept {}

	explicit LightCursor(const AnyRecordList &_list) noexcept
		:list(_list) {}

	/**
	 * Rewind to the first record.
	 */
	void Rewind() noexcept {
		next = list.First();
	}

	/**
	 * If the pointed-to #Record has been deleted, rewind to the
	 * first record.
	 *
	 * @return true if the #Record has been deleted, false if the
	 * call was a no-op
	 */
	bool FixDeleted(uint64_t expected_id) noexcept;

	std::pair<const Record *,
		  const Record *> TimeRange(uint64_t since,
					    uint64_t until) const noexcept {
		return list.TimeRange(since, until);
	}

	void AddAppendListener(AppendListener &l) noexcept {
		list.AddAppendListener(l);
	}

	/**
	 * Does this instance point to a valid record?
	 */
	operator bool() const noexcept {
		return next != nullptr;
	}

	const Record &operator*() const noexcept {
		assert(next != nullptr);

		return *next;
	}

	const Record *operator->() const noexcept {
		assert(next != nullptr);

		return next;
	}

	/**
	 * Skip to the next record.
	 */
	LightCursor &operator++() noexcept {
		assert(next != nullptr);

		next = list.Next(*next);
		return *this;
	}

protected:
	const Record *First() const noexcept;

	void SetNext(const Record &record) noexcept {
		next = &record;
	}
};
