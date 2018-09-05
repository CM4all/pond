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

#include "AnyList.hxx"
#include "RList.hxx"

std::pair<const Record *, const Record *>
AnyRecordList::TimeRange(uint64_t since,
			 uint64_t until) const noexcept
{
	return all != nullptr
		? all->TimeRange(since, until)
		: (per_site != nullptr
		   ? per_site->TimeRange(since, until)
		   : std::make_pair(nullptr, nullptr));
}

const Record *
AnyRecordList::First() const noexcept
{
	return all != nullptr
		? all->First()
		: (per_site != nullptr
		   ? per_site->First()
		   : nullptr);
}

const Record *
AnyRecordList::Next(const Record &r) const noexcept
{
	return all != nullptr
		? all->Next(r)
		: per_site->Next(r);
}

void
AnyRecordList::AddAppendListener(AppendListener &l) const noexcept
{
	if (all != nullptr)
		all->AddAppendListener(l);
	else if (per_site != nullptr)
		per_site->AddAppendListener(l);
}
