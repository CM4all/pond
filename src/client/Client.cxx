// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Client.hxx"
#include "Datagram.hxx"
#include "system/Error.hxx"

#include <sys/socket.h>

void
PondClient::FillInputBuffer()
{
	auto w = input.Write();
	if (w.empty())
		throw std::runtime_error("Input buffer is full");

	auto nbytes = recv(fd.Get(), w.data(), w.size(), 0);
	if (nbytes < 0)
		throw MakeErrno("Failed to receive");

	if (nbytes == 0)
		throw std::runtime_error("Premature end of stream");

	input.Append(nbytes);
}

const void *
PondClient::FullReceive(size_t size)
{
	while (true) {
		auto r = input.Read();
		if (r.size() >= size)
			return r.data();

		FillInputBuffer();
	}
}

void
PondClient::FullReceive(void *dest, size_t size)
{
	memcpy(dest, FullReceive(size), size);
	input.Consume(size);
}

PondDatagram
PondClient::Receive()
{
	PondHeader header;
	FullReceive(&header, sizeof(header));

	PondDatagram d;
	d.id = FromBE16(header.id);
	d.command = PondResponseCommand(FromBE16(header.command));
	d.payload.size = FromBE16(header.size);

	if (d.payload.size > 0) {
		d.payload.data.reset(new std::byte[d.payload.size]);
		FullReceive(d.payload.data.get(), d.payload.size);
	}

	return d;
}
