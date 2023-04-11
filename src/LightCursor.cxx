// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LightCursor.hxx"
#include "Record.hxx"

bool
LightCursor::FixDeleted(uint64_t expected_id) noexcept
{
	if (next == nullptr)
		return false;

	const auto *first = list.First();
	if (first != next &&
	    (first == nullptr || expected_id < first->GetId())) {
		next = first;
		return true;
	} else
		return false;
}
