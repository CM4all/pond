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

#include "Cursor.hxx"
#include "Record.hxx"

bool
Cursor::FixDeleted() noexcept
{
	if (*this && LightCursor::FixDeleted(id)) {
		CheckUpdateId();
		return true;
	} else
		return false;
}

void
Cursor::SetNext(const Record &record) noexcept
{
	LightCursor::SetNext(record);
	id = record.GetId();
}

void
Cursor::Rewind() noexcept
{
	LightCursor::Rewind();
	CheckUpdateId();
}

void
Cursor::OnAppend(const Record &record) noexcept
{
	assert(!*this);

	SetNext(record);
	id = record.GetId();
}

Cursor &
Cursor::operator++() noexcept
{
	assert(*this);

	LightCursor::operator++();
	CheckUpdateId();

	return *this;
}

inline void
Cursor::CheckUpdateId() noexcept
{
	if (*this)
		id = LightCursor::operator*().GetId();
}
