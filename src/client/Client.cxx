// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Client.hxx"
#include "Datagram.hxx"
#include "net/SocketError.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/SpanCast.hxx"

#include <algorithm> // for std::copy_n()

#include <sys/socket.h>

void
PondClient::FillInputBuffer()
{
	auto w = input.Write();
	if (w.empty())
		throw SocketBufferFullError{};

	auto nbytes = fd.Receive(w);
	if (nbytes < 0)
		throw MakeSocketError("Failed to receive");

	if (nbytes == 0)
		throw SocketClosedPrematurelyError{};

	input.Append(nbytes);
}

const std::byte *
PondClient::FullReceive(std::size_t size)
{
	while (true) {
		auto r = input.Read();
		if (r.size() >= size)
			return r.data();

		FillInputBuffer();
	}
}

void
PondClient::FullReceive(std::span<std::byte> dest)
{
	std::copy_n(FullReceive(dest.size()), dest.size(), dest.begin());
	input.Consume(dest.size());
}

PondDatagram
PondClient::Receive()
{
	PondHeader header;
	FullReceive(ReferenceAsWritableBytes(header));

	PondDatagram d;
	d.id = FromBE16(header.id);
	d.command = PondResponseCommand(FromBE16(header.command));
	d.payload.size = FromBE16(header.size);

	if (d.payload.size > 0) {
		d.payload.data.reset(new std::byte[d.payload.size]);
		FullReceive({d.payload.data.get(), d.payload.size});
	}

	return d;
}
