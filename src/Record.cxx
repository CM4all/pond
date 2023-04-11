// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Record.hxx"
#include "net/log/Parser.hxx"

#include <algorithm>

#include <string.h>

Record::Record(uint64_t _id, std::span<const std::byte> _raw)
	:id(_id), raw_size(_raw.size())
{
	memcpy((void *)(this + 1), _raw.data(), raw_size);

	parsed = Net::Log::ParseDatagram(GetRaw());
}
