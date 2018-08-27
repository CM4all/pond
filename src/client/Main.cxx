/*
 * Copyright 2017-2018 Content Management AG
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

#include "Protocol.hxx"
#include "Datagram.hxx"
#include "Client.hxx"
#include "Open.hxx"
#include "Filter.hxx"
#include "avahi/Check.hxx"
#include "system/Error.hxx"
#include "net/SendMessage.hxx"
#include "net/log/String.hxx"
#include "net/log/Parser.hxx"
#include "net/log/Datagram.hxx"
#include "net/log/OneLine.hxx"
#include "time/ISO8601.hxx"
#include "time/Convert.hxx"
#include "util/ConstBuffer.hxx"
#include "util/PrintException.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "util/ByteOrder.hxx"
#include "util/Macros.hxx"

#include <stdlib.h>
#include <poll.h>

gcc_pure
static const char *
IsFilter(const char *arg, StringView name) noexcept
{
	return StringStartsWith(arg, name) && arg[name.size] == '='
		? arg + name.size + 1
		: nullptr;
}

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

static std::chrono::system_clock::time_point
ParseLocalDate(const char *s)
{
	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	const char *end = strptime(s, "%F", &tm);
	if (end == nullptr || *end != 0)
		throw std::runtime_error("Failed to parse date");

	return MakeTime(tm);
}

static void
ParseFilterItem(Filter &filter, PondGroupSitePayload &group_site, bool &follow,
		const char *p)
{
	if (auto value = IsFilter(p, "site")) {
		if (group_site.max_sites != 0)
			throw "site and group_site are mutually exclusive";

		if (*value == 0)
			throw "Site name must not be empty";

		auto e = filter.sites.emplace(value);
		if (!e.second)
			throw "Duplicate site name";
	} else if (auto group_value = IsFilter(p, "group_site")) {
		if (!filter.sites.empty())
			throw "site and group_site are mutually exclusive";

		if (group_site.max_sites != 0)
			throw "Duplicate group_site";

		char *endptr;
		auto max = strtoul(group_value, &endptr, 10);
		if (endptr == group_value)
			throw "Number expected after group_site=";
		if (max == 0)
			throw "group_site max must be positive";

		if (*endptr == '@') {
			group_value = endptr + 1;
			auto skip = strtoul(group_value, &endptr, 10);
			if (endptr == group_value)
				throw "Number expected after group_site=...@";

			group_site.skip_sites = ToBE32(skip);
		} else if (*endptr != 0)
			throw "Garbage after group_site max";

		group_site.max_sites = ToBE32(max);
	} else if (auto since = IsFilter(p, "since"))
		filter.since = Net::Log::Datagram::ExportTimestamp(ParseISO8601(since));
	else if (auto until = IsFilter(p, "until"))
		filter.until = Net::Log::Datagram::ExportTimestamp(ParseISO8601(until));
	else if (auto date_string = IsFilter(p, "date")) {
		const auto date = ParseLocalDate(date_string);
		filter.since = Net::Log::Datagram::ExportTimestamp(date);
		filter.until = Net::Log::Datagram::ExportTimestamp(date + std::chrono::hours(24));
	} else if (auto type_string = IsFilter(p, "type")) {
		filter.type = Net::Log::ParseType(type_string);
		if (filter.type == Net::Log::Type::UNSPECIFIED)
			throw "Bad type filter";
	} else if (StringIsEqual(p, "--follow"))
		follow = true;
	else
		throw "Unrecognized query argument";
}

static void
Query(const PondServerSpecification &server, ConstBuffer<const char *> args)
{
	Filter filter;
	PondGroupSitePayload group_site{0, 0};
	bool follow = false;

	const FileDescriptor out_fd(STDOUT_FILENO);
	auto socket = CheckPacketSocket(out_fd);

	while (!args.empty()) {
		const char *p = args.shift();
		ParseFilterItem(filter, group_site, follow, p);
	}

	PondClient client(PondConnect(server));
	const auto id = client.MakeId();
	client.Send(id, PondRequestCommand::QUERY);

	if (filter.type != Net::Log::Type::UNSPECIFIED)
		client.SendT(id, PondRequestCommand::FILTER_TYPE, filter.type);

	for (const auto &i : filter.sites)
		client.Send(id, PondRequestCommand::FILTER_SITE, i);
	const bool single_site = filter.sites.begin() != filter.sites.end() &&
		std::next(filter.sites.begin()) == filter.sites.end();

	if (filter.since != 0)
		client.Send(id, PondRequestCommand::FILTER_SINCE, filter.since);

	if (filter.until != 0)
		client.Send(id, PondRequestCommand::FILTER_UNTIL, filter.until);

	if (group_site.max_sites != 0)
		client.SendT(id, PondRequestCommand::GROUP_SITE, group_site);

	if (follow)
		client.Send(id, PondRequestCommand::FOLLOW);

	client.Send(id, PondRequestCommand::COMMIT);

	struct pollfd pfds[] = {
		{
			/* waiting for messages from the Pond
			   server */
			.fd = client.GetSocket().Get(),
			.events = POLLIN,
			.revents = 0,
		},
		{
			.fd = out_fd.Get(),
			/* only waiting for POLLERR, which is an
			   output-only flag */
			.events = 0,
			.revents = 0,
		},
	};

	while (true) {
		if (client.IsEmpty()) {
			if (poll(pfds, ARRAY_SIZE(pfds), -1) < 0)
				throw MakeErrno("poll() failed");

			if (pfds[1].revents)
				/* the output pipe/socket was closed (probably
				   POLLERR), and there's no point in waiting
				   for more data from the Pond server */
				break;
		}

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
			if (socket.IsDefined()) {
				/* if fd2 is a packet socket, send raw
				   datagrams to it */
				SendPacket(socket, d.payload);
				continue;
			}

			try {
				LogOneLine(out_fd,
					   Net::Log::ParseDatagram(d.payload.data.get(),
								   d.payload.data.get() + d.payload.size),
					   !single_site);
			} catch (Net::Log::ProtocolError) {
				fprintf(stderr, "Failed to parse log record\n");
			}

			break;
		}
	}
}

static void
Clone(const PondServerSpecification &server, ConstBuffer<const char *> args)
{
	if (args.size != 1)
		throw "Bad arguments";

	const char *other_server = args.front();

	PondClient client(PondConnect(server));
	const auto id = client.MakeId();
	client.Send(id, PondRequestCommand::CLONE, other_server);
	client.Send(id, PondRequestCommand::COMMIT);

	while (true) {
		const auto d = client.Receive();
		if (d.id != id)
			continue;

		switch (d.command) {
		case PondResponseCommand::NOP:
			break;

		case PondResponseCommand::ERROR:
			throw FormatRuntimeError("Server error: %s",
						 d.payload.ToString().c_str());

		case PondResponseCommand::END:
			return;

		case PondResponseCommand::LOG_RECORD:
			throw "Unexpected response packet";
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
			"\n"
			"Commands:\n"
			"  query [--follow] [type=http_access|http_error|submission] [site=VALUE] [group_site=MAX[@SKIP]] [since=ISO8601] [until=ISO8601] [date=YYYY-MM-DD]\n"
			"  clone OTHERSERVER[:PORT]\n",
			argv[0]);
		return EXIT_FAILURE;
	}

	PondServerSpecification server;
	server.host = args.shift();
	if (auto zs = StringAfterPrefix(server.host, "zeroconf/"))
		server.zeroconf_service = MakeZeroconfServiceType(zs, "_tcp");

	const char *const command = args.shift();

	if (StringIsEqual(command, "query")) {
		Query(server, args);
		return EXIT_SUCCESS;
	} else if (StringIsEqual(command, "clone")) {
		Clone(server, args);
		return EXIT_SUCCESS;
	} else {
		fprintf(stderr, "Unknown command: %s\n", command);
		return EXIT_FAILURE;
	}
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
