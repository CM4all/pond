// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
