/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Client.hxx"
#include "Datagram.hxx"
#include "system/Error.hxx"

#include <sys/socket.h>

void
PondClient::Send(uint16_t id, PondRequestCommand command,
		 ConstBuffer<void> payload)
{
	PondHeader header;
	if (payload.size >= std::numeric_limits<decltype(header.size)>::max())
		throw std::runtime_error("Payload is too large");

	header.id = ToBE16(id);
	header.command = ToBE16(uint16_t(command));
	header.size = ToBE16(payload.size);

	struct iovec vec[] = {
		{
			.iov_base = &header,
			.iov_len = sizeof(header),
		},
		{
			.iov_base = const_cast<void *>(payload.data),
			.iov_len = payload.size,
		},
	};

	struct msghdr m = {
		.msg_name = nullptr,
		.msg_namelen = 0,
		.msg_iov = vec,
		.msg_iovlen = 1u + !payload.empty(),
		.msg_control = nullptr,
		.msg_controllen = 0,
		.msg_flags = 0,
	};

	ssize_t nbytes = sendmsg(fd.Get(), &m, 0);
	if (nbytes < 0)
		throw MakeErrno("Failed to send");

	if (size_t(nbytes) != sizeof(header) + payload.size)
		throw std::runtime_error("Short send");
}

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
