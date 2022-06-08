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

#include "LightCursor.hxx"

#include <assert.h>

/**
 * An iterator for records in the #Database.  While an instance
 * exists, the database may be modified, because FixDeleted() will
 * take care for cleaning up invalid pointers.
 */
class Cursor
	: LightCursor {

	uint64_t id;

public:
	explicit Cursor(std::nullptr_t n) noexcept
		:LightCursor(n) {}

	explicit Cursor(const AnyRecordList &_list) noexcept
		:LightCursor(_list) {}

	LightCursor ToLightCursor() const noexcept {
		return *this;
	}

	/**
	 * If the pointed-to #Record has been deleted, rewind to the
	 * first record.
	 *
	 * @return true if the #Record has been deleted, false if the
	 * call was a no-op
	 */
	bool FixDeleted() noexcept;

	using LightCursor::TimeLowerBound;
	using LightCursor::AddAppendListener;

	void SetNext(const Record &record) noexcept;

	/**
	 * Rewind to the first record.
	 */
	void Rewind() noexcept;

	/**
	 * Does this instance point to a valid record?
	 */
	using LightCursor::operator bool;
	using LightCursor::operator*;
	using LightCursor::operator->;

	/**
	 * Skip to the next record.
	 */
	Cursor &operator++() noexcept;

	void OnAppend(const Record &record) noexcept;

private:
	void CheckUpdateId() noexcept;
};
