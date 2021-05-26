/*
 * Copyright 2017-2021 CM4all GmbH
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

	auto nbytes = recv(fd.Get(), w.data, w.size, 0);
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
		if (r.size >= size)
			return r.data;

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
		d.payload.data.reset(new uint8_t[d.payload.size]);
		FullReceive(d.payload.data.get(), d.payload.size);
	}

	return d;
}
