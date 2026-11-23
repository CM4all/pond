// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "client/Client.hxx"
#include "client/Datagram.hxx"
#include "client/Open.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/PrintException.hxx"
#include "util/SpanCast.hxx"
#include "Database.hxx"

#include <fmt/format.h>

#include <stdexcept>

#include <stdlib.h>

using std::string_view_literals::operator""sv;

static std::string
ToString(std::span<const std::byte> b) noexcept
{
	return std::string{ToStringView(b)};
}

static void
Clone(Database &db, PondClient &client)
{
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
			throw std::runtime_error(ToString(d.payload));

		case PondResponseCommand::END:
			return;

		case PondResponseCommand::LOG_RECORD:
			db.Emplace(d.payload);
			break;

		case PondResponseCommand::STATS:
			throw SocketProtocolError{"Unexpected response packet"};
		}
	}
}

static void
Clone(Database &db, const PondServerSpecification &spec)
{
	PondClient client{PondConnect(spec)};
	Clone(db, client);
}

int
main(int argc, char **argv)
try {
	std::span<const char *const> args{argv + 1, static_cast<std::size_t>(argc - 1)};
	if (argc != 2) {
		fmt::print(stderr, "Usage: {} ADDRESS\n"
			   "\n"
			   "Clone the database of the specified server.\n",
			   argv[0]);
		return EXIT_FAILURE;
	}

	Database db{4_GiB};
	Clone(db, {.host = argv[1]});

	fmt::print("memory_capacity={}\n"
		   "memory_usage={}\n"
		   "n_records={}\n",
		   db.GetMemoryCapacity(),
		   db.GetMemoryUsage(),
		   db.GetRecordCount());

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
