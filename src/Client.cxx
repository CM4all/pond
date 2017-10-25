/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Protocol.hxx"
#include "system/Error.hxx"
#include "net/RConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/ConstBuffer.hxx"
#include "util/PrintException.hxx"
#include "util/StringAPI.hxx"
#include "util/ByteOrder.hxx"

#include <memory>

#include <sys/socket.h>
#include <stdlib.h>

struct PondDatagram {
	uint16_t id;
	PondResponseCommand command;

	struct {
		std::unique_ptr<uint8_t[]> data;
		size_t size;

		std::string ToString() const {
			return std::string((const char *)data.get(), size);
		}
	} payload;
};

class PondClient {
	UniqueSocketDescriptor fd;

	uint16_t last_id = 0;

public:
	explicit PondClient(const char *server)
		:fd(ResolveConnectStreamSocket(server, 5480)) {
		fd.SetBlocking();
	}

	uint16_t MakeId() {
		return ++last_id;
	}

	void Send(uint16_t id, PondRequestCommand command,
		  ConstBuffer<void> payload=nullptr);

	PondDatagram Receive();
};

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

static void
FullReceive(SocketDescriptor fd, uint8_t *buffer, size_t size)
{
	while (size > 0) {
		ssize_t nbytes = recv(fd.Get(), buffer, size, 0);
		if (nbytes < 0)
			throw MakeErrno("Failed to receive");

		if (nbytes == 0)
			throw std::runtime_error("Premature end of stream");

		buffer += nbytes;
		size -= nbytes;
	}
}

PondDatagram
PondClient::Receive()
{
	PondHeader header;
	ssize_t nbytes = recv(fd.Get(), &header, sizeof(header), 0);
	if (nbytes < 0)
		throw MakeErrno("Failed to receive");

	if (nbytes == 0)
		throw std::runtime_error("Premature end of stream");

	if (size_t(nbytes) != sizeof(header))
		throw std::runtime_error("Short receive");

	PondDatagram d;
	d.id = FromBE16(header.id);
	d.command = PondResponseCommand(FromBE16(header.command));
	d.payload.size = FromBE16(header.size);

	if (d.payload.size > 0) {
		d.payload.data.reset(new uint8_t[d.payload.size]);
		FullReceive(fd, d.payload.data.get(), d.payload.size);
	}

	return d;
}

static void
Query(const char *server, ConstBuffer<const char *> args)
{
	if (!args.empty())
		throw "Too many arguments";

	PondClient client(server);
	const auto id = client.MakeId();
	client.Send(id, PondRequestCommand::QUERY);
	client.Send(id, PondRequestCommand::COMMIT);

	while (true) {
		const auto d = client.Receive();
		if (d.id != id)
			continue;

		switch (d.command) {
		case PondResponseCommand::NOP:
			break;

		case PondResponseCommand::ERROR:
			throw std::runtime_error(d.payload.ToString());

		case PondResponseCommand::END:
			return;

		case PondResponseCommand::LOG_RECORD:
			// TODO: deserialize and dump
			printf("log_record\n");
			break;
		}
	}
}

int
main(int argc, char **argv)
try {
	ConstBuffer<const char *> args(argv + 1, argc - 1);
	if (args.size < 2) {
		fprintf(stderr, "Usage: %s SERVER[:PORT] COMMAND ...\n"
			"\n"
			"Commands:\n"
			"  query\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *const server = args.shift();
	const char *const command = args.shift();

	if (StringIsEqual(command, "query")) {
		Query(server, args);
		return EXIT_SUCCESS;
	} else {
		fprintf(stderr, "Unknown command: %s\n", command);
		return EXIT_FAILURE;
	}
} catch (const char *msg) {
	fprintf(stderr, "%s\n", msg);
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
