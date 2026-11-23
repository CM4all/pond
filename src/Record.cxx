// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Record.hxx"
#include "net/log/Parser.hxx"

#include <algorithm>

inline std::span<const std::byte>
Record::CopyFrom(std::span<const std::byte> src) noexcept
{
	std::byte *dest = reinterpret_cast<std::byte *>(this + 1);
	std::byte *end = std::copy(src.begin(), src.end(), dest);
	return {dest, end};
}

Record::Record(uint64_t _id, std::span<const std::byte> _raw)
	:id(_id), raw_size(_raw.size()),
	 parsed(Net::Log::ParseDatagram(CopyFrom(_raw)))
{
	if (_raw.size() > std::numeric_limits<uint_least16_t>::max())
		throw Net::Log::ProtocolError{};
}
