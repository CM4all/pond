// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Protocol.hxx"
#include "net/log/Chrono.hxx"
#include "net/SocketDescriptor.hxx"
#include "util/SpanCast.hxx"
#include "util/ByteOrder.hxx"

#include <span>
#include <string>

struct PondDatagram;

void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		std::span<const std::byte> payload={});

template<typename T>
static inline void
SendPondRequestT(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		 const T &payload)
{
	SendPondRequest(s, id, command, ReferenceAsBytes(payload));
}

static inline void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		std::string_view payload)
{
	SendPondRequest(s, id, command, AsBytes(payload));
}

static inline void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		const char *payload)
{
	SendPondRequest(s, id, command, std::string_view{payload});
}

static inline void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		uint64_t payload)
{
	const uint64_t be = ToBE64(payload);
	SendPondRequestT(s, id, command, be);
}

static inline void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		Net::Log::Duration payload)
{
	SendPondRequest(s, id, command, payload.count());
}

static inline void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		Net::Log::TimePoint payload)
{
	SendPondRequest(s, id, command, payload.time_since_epoch());
}
