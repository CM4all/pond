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

#include "Protocol.hxx"
#include "net/log/Chrono.hxx"
#include "net/SocketDescriptor.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"
#include "util/ByteOrder.hxx"

#include <string>

struct PondDatagram;

void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		ConstBuffer<void> payload=nullptr);

template<typename T>
static inline void
SendPondRequestT(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		 const T &payload)
{
	SendPondRequest(s, id, command,
			ConstBuffer<void>(&payload, sizeof(payload)));
}

static inline void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		StringView payload)
{
	SendPondRequest(s, id, command, payload.ToVoid());
}

static inline void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		const std::string &payload)
{
	SendPondRequest(s, id, command,
			StringView(payload.data(), payload.size()));
}

static inline void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		const char *payload)
{
	SendPondRequest(s, id, command, StringView(payload));
}

static inline void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		uint64_t payload)
{
	uint64_t be = ToBE64(payload);
	SendPondRequest(s, id, command, ConstBuffer<void>(&be, sizeof(be)));
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
