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

#include "Send.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/ByteOrder.hxx"
#include "util/StaticFifoBuffer.hxx"

struct PondDatagram;

class PondClient {
	UniqueSocketDescriptor fd;

	uint16_t last_id = 0;

	StaticFifoBuffer<uint8_t, 256 * 1024> input;

public:
	explicit PondClient(UniqueSocketDescriptor &&_fd) noexcept
		:fd(std::move(_fd)) {
		fd.SetBlocking();
	}

	SocketDescriptor GetSocket() {
		return fd;
	}

	uint16_t MakeId() {
		return ++last_id;
	}

	void Send(uint16_t id, PondRequestCommand command) {
		SendPondRequest(fd, id, command);
	}

	template<typename T>
	void Send(uint16_t id, PondRequestCommand command, T &&payload) {
		SendPondRequest(fd, id, command, std::forward<T>(payload));
	}

	template<typename T>
	void SendT(uint16_t id, PondRequestCommand command,
		   const T &payload) {
		SendPondRequestT(fd, id, command, payload);
	}

	bool IsEmpty() const noexcept {
		return input.empty();
	}

	PondDatagram Receive();

private:
	void FillInputBuffer();
	const void *FullReceive(size_t size);
	void FullReceive(void *dest, size_t size);
};
