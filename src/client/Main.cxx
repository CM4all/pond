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

#include "Protocol.hxx"
#include "Datagram.hxx"
#include "Client.hxx"
#include "ResultWriter.hxx"
#include "Open.hxx"
#include "Filter.hxx"
#include "avahi/Check.hxx"
#include "system/Error.hxx"
#include "net/log/String.hxx"
#include "net/log/Parser.hxx"
#include "time/Parser.hxx"
#include "time/Math.hxx"
#include "time/Convert.hxx"
#include "util/ConstBuffer.hxx"
#include "util/PrintException.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "util/ByteOrder.hxx"
#include "util/StaticFifoBuffer.hxx"

#include <inttypes.h>
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

struct QueryOptions {
	const char *per_site_append = nullptr;

	bool follow = false;
	bool raw = false;
};

static void
ParseFilterItem(Filter &filter, PondGroupSitePayload &group_site,
		QueryOptions &options,
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
			max = std::numeric_limits<decltype(group_site.max_sites)>::max();
		else if (max == 0)
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
	} else if (auto since = IsFilter(p, "since")) {
		auto t = ParseTimePoint(since);
		filter.since = Net::Log::FromSystem(t.first);
	} else if (auto until = IsFilter(p, "until")) {
		auto t = ParseTimePoint(until);
		filter.until = Net::Log::FromSystem(t.first + t.second);
	} else if (auto time = IsFilter(p, "time")) {
		auto t = ParseTimePoint(time);
		filter.since = Net::Log::FromSystem(t.first);
		filter.until = Net::Log::FromSystem(t.first + t.second);
	} else if (auto date_string = IsFilter(p, "date")) {
		const auto date = ParseLocalDate(date_string);
		filter.since = Net::Log::FromSystem(date);
		filter.until = Net::Log::FromSystem(date + std::chrono::hours(24));
	} else if (StringIsEqual(p, "today")) {
		const auto midnight =
			PrecedingMidnightLocal(std::chrono::system_clock::now());
		filter.since = Net::Log::FromSystem(midnight);
		filter.until = Net::Log::FromSystem(midnight + std::chrono::hours(24));
	} else if (auto type_string = IsFilter(p, "type")) {
		filter.type = Net::Log::ParseType(type_string);
		if (filter.type == Net::Log::Type::UNSPECIFIED)
			throw "Bad type filter";
	} else if (auto per_site_append = StringAfterPrefix(p, "--per-site-append=")) {
		options.per_site_append = per_site_append;
	} else if (StringIsEqual(p, "--follow"))
		options.follow = true;
	else if (StringIsEqual(p, "--raw"))
		options.raw = true;
	else
		throw "Unrecognized query argument";
}

static void
Query(const PondServerSpecification &server, ConstBuffer<const char *> args)
{
	Filter filter;
	PondGroupSitePayload group_site{0, 0};
	QueryOptions options;

	while (!args.empty()) {
		const char *p = args.shift();
		try {
			ParseFilterItem(filter, group_site, options, p);
		} catch (...) {
			std::throw_with_nested(FormatRuntimeError("Failed to parse '%s'", p));
		}
	}

	if (options.per_site_append != nullptr &&
	    filter.sites.empty() &&
	    group_site.max_sites == 0) {
		/* auto-enable GROUP_SITE if "--per-site-append" was
		   used */
		group_site.max_sites = ToBE32(std::numeric_limits<decltype(group_site.max_sites)>::max());
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

	ResultWriter result_writer(options.raw, single_site,
				   options.per_site_append);

	if (filter.since != Net::Log::TimePoint::min())
		client.Send(id, PondRequestCommand::FILTER_SINCE, filter.since);

	if (filter.until != Net::Log::TimePoint::max())
		client.Send(id, PondRequestCommand::FILTER_UNTIL, filter.until);

	if (group_site.max_sites != 0)
		client.SendT(id, PondRequestCommand::GROUP_SITE, group_site);

	if (options.follow)
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
			.fd = result_writer.GetFileDescriptor().Get(),
			/* only waiting for POLLERR, which is an
			   output-only flag */
			.events = 0,
			.revents = 0,
		},
	};

	while (true) {
		if (client.IsEmpty()) {
			if (poll(pfds, std::size(pfds), -1) < 0)
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
			throw FormatRuntimeError("Server error: %s",
						 d.payload.ToString().c_str());

		case PondResponseCommand::END:
			result_writer.Flush();
			return;

		case PondResponseCommand::LOG_RECORD:
			try {
				result_writer.Write(d.payload);
			} catch (Net::Log::ProtocolError) {
				fprintf(stderr, "Failed to parse log record\n");
			}

			break;

		case PondResponseCommand::STATS:
			throw "Unexpected response packet";
		}
	}

	result_writer.Flush();
}

static void
Stats(const PondServerSpecification &server, ConstBuffer<const char *> args)
{
	if (!args.empty())
		throw "Bad arguments";

	PondClient client(PondConnect(server));
	const auto id = client.MakeId();
	client.Send(id, PondRequestCommand::STATS);
	const auto response = client.Receive();
	if (response.id != id)
		throw "Wrong id";

	if (response.command != PondResponseCommand::STATS)
		throw "Wrong response command";

	ConstBuffer<void> payload = response.payload;
	const PondStatsPayload &stats = *(const PondStatsPayload *)payload.data;

	if (payload.size < sizeof(PondStatsPayload))
		// TODO: backwards compatibility, allow smaller payloads
		throw "Wrong response payload size";

	printf("memory_capacity=%" PRIu64 "\n"
	       "memory_usage=%" PRIu64 "\n"
	       "n_records=%" PRIu64 "\n",
	       FromBE64(stats.memory_capacity),
	       FromBE64(stats.memory_usage),
	       FromBE64(stats.n_records));

	printf("n_received=%" PRIu64 "\n"
	       "n_malformed=%" PRIu64 "\n"
	       "n_discarded=%" PRIu64 "\n",
	       FromBE64(stats.n_received),
	       FromBE64(stats.n_malformed),
	       FromBE64(stats.n_discarded));
}

template<typename B>
static size_t
ReadToBuffer(FileDescriptor fd, B &buffer)
{
	auto w = buffer.Write();
	if (w.empty())
		throw std::runtime_error("Input buffer full");

	auto nbytes = fd.Read(w.data, w.size);
	if (nbytes < 0)
		throw MakeErrno("Failed to read");

	buffer.Append(nbytes);
	return nbytes;
}

template<typename F>
static void
ReadPackets(FileDescriptor fd, F &&f)
{
	StaticFifoBuffer<uint8_t, 65536> input;

	while (true) {
		auto r = input.Read();
		// TODO: alignment/padding?
		const auto &header = *(const PondHeader *)(const void *)r.data;
		if (r.size < sizeof(header) ||
		    r.size < sizeof(header) + FromBE16(header.size)) {
			size_t nbytes = ReadToBuffer(fd, input);
			if (nbytes == 0) {
				if (!input.empty())
					throw std::runtime_error("Trailing garbage");
				return;
			}

			continue;
		}

		f(FromBE16(header.id), FromBE16(header.command),
		  ConstBuffer<void>(&header + 1, FromBE16(header.size)));
		input.Consume(sizeof(header) + FromBE16(header.size));
	}


}

static void
Inject(const PondServerSpecification &server, ConstBuffer<const char *> args)
{
	if (!args.empty())
		throw "Bad arguments";

	PondClient client(PondConnect(server));

	ReadPackets(FileDescriptor(STDIN_FILENO), [&client](unsigned, unsigned command, ConstBuffer<void> payload){
			if (command == unsigned(PondResponseCommand::LOG_RECORD))
				client.Send(client.MakeId(),
					    PondRequestCommand::INJECT_LOG_RECORD,
					    payload);
		});
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
		case PondResponseCommand::STATS:
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
			"  query [--follow] [--raw] [type=http_access|http_error|submission] [site=VALUE] [group_site=[MAX][@SKIP]] [since=ISO8601] [until=ISO8601] [date=YYYY-MM-DD] [today]\n"
			"  stats\n"
			"  inject <RAWFILE\n"
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
	} else if (StringIsEqual(command, "stats")) {
		Stats(server, args);
		return EXIT_SUCCESS;
	} else if (StringIsEqual(command, "inject")) {
		Inject(server, args);
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
