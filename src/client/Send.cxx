// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Send.hxx"
#include "io/Iovec.hxx"
#include "net/SendMessage.hxx"
#include "system/Error.hxx"

void
SendPondRequest(SocketDescriptor s, uint16_t id, PondRequestCommand command,
		std::span<const std::byte> payload)
{
	PondHeader header;
	if (payload.size() >= std::numeric_limits<decltype(header.size)>::max())
		throw std::runtime_error("Payload is too large");

	header.id = ToBE16(id);
	header.command = ToBE16(uint16_t(command));
	header.size = ToBE16(payload.size());

	const struct iovec vec[] = {
		MakeIovecT(header),
		MakeIovec(payload),
	};

	auto nbytes = SendMessage(s, std::span{vec, 1u + !payload.empty()}, 0);
	if (size_t(nbytes) != sizeof(header) + payload.size())
		throw std::runtime_error("Short send");
}
