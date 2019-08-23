/*
 * Copyright 2017-2019 Content Management AG
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

#include "ResultWriter.hxx"
#include "Protocol.hxx"
#include "system/Error.hxx"
#include "net/SendMessage.hxx"
#include "net/log/Datagram.hxx"
#include "net/log/OneLine.hxx"
#include "net/log/Parser.hxx"
#include "util/ByteOrder.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Macros.hxx"

/**
 * Cast this #FileDescriptor to a #SocketDescriptor if it specifies a
 * socket.
 */
gcc_pure
static SocketDescriptor
CheckSocket(FileDescriptor fd)
{
	return fd.IsSocket()
		? SocketDescriptor::FromFileDescriptor(fd)
		: SocketDescriptor::Undefined();
}

/**
 * Cast this #FileDescriptor to a #SocketDescriptor if it specifies a
 * packet socket (SOCK_DGRAM or SOCK_SEQPACKET).
 */
gcc_pure
static SocketDescriptor
CheckPacketSocket(FileDescriptor fd)
{
	auto s = CheckSocket(fd);
	if (s.IsDefined() && s.IsStream())
		s.SetUndefined();
	return s;
}

static void
SendPacket(SocketDescriptor s, ConstBuffer<void> payload)
{
	struct iovec vec[] = {
		{
			.iov_base = const_cast<void *>(payload.data),
			.iov_len = payload.size,
		},
	};

	SendMessage(s, ConstBuffer<struct iovec>(vec), 0);
}

ResultWriter::ResultWriter(bool _raw, bool _single_site) noexcept
	:fd(STDOUT_FILENO),
	 socket(CheckPacketSocket(fd)),
	 raw(_raw), single_site(_single_site)
{
}

void
ResultWriter::Write(ConstBuffer<void> payload) const
{
	if (socket.IsDefined()) {
		/* if fd2 is a packet socket, send raw
		   datagrams to it */
		SendPacket(socket, payload);
		return;
	} else if (raw) {
		const uint16_t id = 1;
		const auto command = PondResponseCommand::LOG_RECORD;
		const PondHeader header{ToBE16(id), ToBE16(uint16_t(command)), ToBE16(payload.size)};
		if (write(STDOUT_FILENO, &header, sizeof(header)) < 0 ||
		    write(STDOUT_FILENO, payload.data, payload.size) < 0)
			throw MakeErrno("Failed to write to stdout");
		return;
	}

	LogOneLine(fd,
		   Net::Log::ParseDatagram(payload),
		   !single_site);
}
