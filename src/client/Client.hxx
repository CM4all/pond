/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Protocol.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/RConnectSocket.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"
#include "util/ByteOrder.hxx"

#include <string>

struct PondDatagram;

class PondClient {
	UniqueSocketDescriptor fd;

	uint16_t last_id = 0;

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

	PondDatagram Receive();
};
