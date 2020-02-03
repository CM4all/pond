/*
 * Copyright 2017-2020 CM4all GmbH
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

#include "Cursor.hxx"
#include "Filter.hxx"

/**
 * A wrapper for #Cursor which applies a #Filter.
 */
class Selection {
	Cursor cursor;

	Filter filter;

public:
	template<typename F>
	Selection(const AnyRecordList &_list, F &&_filter) noexcept
		:cursor(_list),
		 filter(std::forward<F>(_filter)) {}

	bool FixDeleted() noexcept;
	void Rewind() noexcept;

	void AddAppendListener(AppendListener &l) noexcept {
		cursor.AddAppendListener(l);
	}

	operator bool() const noexcept;

	const Record &operator*() const noexcept {
		return *cursor;
	}

	const Record *operator->() const noexcept {
		return cursor.operator->();
	}

	/**
	 * Skip to the next record.
	 */
	Selection &operator++() noexcept;

	Selection &operator+=(size_t n) noexcept {
		while (n-- > 0)
			operator++();
		return *this;
	}

	bool OnAppend(const Record &record) noexcept;

private:
	void SkipMismatches() noexcept;
};
