// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Async.hxx"
#include "Datagram.hxx"
#include "system/Error.hxx"

#include <string.h> // for memcpy()
#include <sys/socket.h>

void
PondAsyncClient::OnSocketReady(unsigned events) noexcept
try {
	if (events & SocketEvent::ERROR)
		throw MakeErrno(GetSocket().GetError(),
				"Socket error");

	if (events & SocketEvent::HANGUP)
		throw std::runtime_error("Hangup");

	FillInputBuffer();

	while (true) {
		auto r = input.Read();
		PondHeader header;
		if (r.size() < sizeof(header))
			/* need more data */
			return;

		/* copy to stack to avoid alignment errors */
		memcpy(&header, r.data(), sizeof(header));
		r = r.subspan(sizeof(header));

		const auto payload = r.first(FromBE16(header.size));
		if (r.size() < payload.size())
			/* need more data */
			return;

		input.Consume(sizeof(header) + payload.size());

		if (!handler.OnPondDatagram(FromBE16(header.id),
					    PondResponseCommand(FromBE16(header.command)),
					    payload))
			return;
	}
} catch (...) {
	handler.OnPondError(std::current_exception());
}

void
PondAsyncClient::FillInputBuffer()
{
	auto w = input.Write();
	if (w.empty())
		throw std::runtime_error("Input buffer is full");

	auto nbytes = GetSocket().Receive(w);
	if (nbytes < 0)
		throw MakeErrno("Failed to receive");

	if (nbytes == 0)
		throw std::runtime_error("Premature end of stream");

	input.Append(nbytes);
}
