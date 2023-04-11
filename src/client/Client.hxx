// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Send.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/ByteOrder.hxx"
#include "util/StaticFifoBuffer.hxx"

struct PondDatagram;

class PondClient {
	UniqueSocketDescriptor fd;

	uint16_t last_id = 0;

	StaticFifoBuffer<std::byte, 256 * 1024> input;

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
