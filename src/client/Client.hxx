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

#pragma once

#include "Protocol.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/RConnectSocket.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"
#include "util/ByteOrder.hxx"
#include "util/StaticFifoBuffer.hxx"

#include <string>

struct PondDatagram;

class PondClient {
	UniqueSocketDescriptor fd;

	uint16_t last_id = 0;

	StaticFifoBuffer<uint8_t, 16384> input;

public:
	explicit PondClient(const char *server)
		:fd(ResolveConnectStreamSocket(server, 5480)) {
		fd.SetBlocking();
	}

	SocketDescriptor GetSocket() {
		return fd;
	}

	uint16_t MakeId() {
		return ++last_id;
	}

	void Send(uint16_t id, PondRequestCommand command,
		  ConstBuffer<void> payload=nullptr);

	template<typename T>
	void SendT(uint16_t id, PondRequestCommand command,
		   const T &payload) {
		Send(id, command,
		     ConstBuffer<void>(&payload, sizeof(payload)));
	}

	void Send(uint16_t id, PondRequestCommand command,
		  StringView payload) {
		Send(id, command, payload.ToVoid());
	}

	void Send(uint16_t id, PondRequestCommand command,
		  const std::string &payload) {
		Send(id, command, StringView(payload.data(), payload.size()));
	}

	void Send(uint16_t id, PondRequestCommand command,
		  const char *payload) {
		Send(id, command, StringView(payload));
	}

	void Send(uint16_t id, PondRequestCommand command,
		  uint64_t payload) {
		uint64_t be = ToBE64(payload);
		Send(id, command, ConstBuffer<void>(&be, sizeof(be)));
	}

	bool IsEmpty() const noexcept {
		return input.IsEmpty();
	}

	PondDatagram Receive();

private:
	void FillInputBuffer();
	const void *FullReceive(size_t size);
	void FullReceive(void *dest, size_t size);
};
