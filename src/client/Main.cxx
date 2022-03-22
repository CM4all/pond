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

#include "Protocol.hxx"
#include "Datagram.hxx"
#include "Client.hxx"
#include "ResultWriter.hxx"
#include "Open.hxx"
#include "Filter.hxx"
#include "lib/avahi/Check.hxx"
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
#include "util/ScopeExit.hxx"
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
	const char *per_site = nullptr;
	const char *per_site_filename = nullptr;

	bool follow = false;
	bool raw = false;
	bool gzip = false;
	bool geoip = false;
	bool anonymize = false;
	bool track_visitors = false;
	bool per_site_nested = false;
};

static void
ParseFilterItem(Filter &filter, PondGroupSitePayload &group_site,
		PondWindowPayload &window,
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
	} else if (auto window_value = IsFilter(p, "window")) {
		if (window.max != 0)
			throw "Duplicate window";

		char *endptr;
		auto max = strtoul(window_value, &endptr, 10);
		if (endptr == window_value)
			max = std::numeric_limits<decltype(window.max)>::max();
		else if (max == 0)
			throw "window max must be positive";

		if (*endptr == '@') {
			window_value = endptr + 1;
			auto skip = strtoul(window_value, &endptr, 10);
			if (endptr == window_value)
				throw "Number expected after window=...@";

			window.skip = ToBE64(skip);
		} else if (*endptr != 0)
			throw "Garbage after window max";

		window.max = ToBE64(max);
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
	} else if (auto per_site = StringAfterPrefix(p, "--per-site=")) {
		options.per_site = per_site;
	} else if (auto per_site_filename = StringAfterPrefix(p, "--per-site-file=")) {
		if (options.per_site == nullptr)
			throw "--per-site-file requires --per-site";
		options.per_site_filename = per_site_filename;
	} else if (StringIsEqual(p, "--per-site-nested")) {
		options.per_site_nested = true;
	} else if (StringIsEqual(p, "--follow"))
		options.follow = true;
	else if (StringIsEqual(p, "--raw"))
		options.raw = true;
	else if (StringIsEqual(p, "--gzip"))
		options.gzip = true;
	else if (StringIsEqual(p, "--geoip"))
		options.geoip = true;
	else if (StringIsEqual(p, "--anonymize"))
		options.anonymize = true;
	else if (StringIsEqual(p, "--track-visitors"))
		options.track_visitors = true;
	else
		throw "Unrecognized query argument";
}

static constexpr auto
MakePollfd(FileDescriptor fd, short events) noexcept
{
	struct pollfd pfd{};
	pfd.fd = fd.Get();
	pfd.events = events;
	return pfd;
}

static void
Query(const PondServerSpecification &server, ConstBuffer<const char *> args)
{
	Filter filter;
	PondGroupSitePayload group_site{0, 0};
	PondWindowPayload window{};
	QueryOptions options;

	while (!args.empty()) {
		const char *p = args.shift();
		try {
			ParseFilterItem(filter, group_site, window,
					options, p);
		} catch (...) {
			std::throw_with_nested(FormatRuntimeError("Failed to parse '%s'", p));
		}
	}

	if (options.per_site != nullptr &&
	    filter.sites.empty() &&
	    group_site.max_sites == 0) {
		/* auto-enable GROUP_SITE if "--per-site" was
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

	GeoIP *geoip_v4 = nullptr, *geoip_v6 = nullptr;

	AtScopeExit(geoip_v4, geoip_v6) {
		if (geoip_v4 != nullptr)
			GeoIP_delete(geoip_v4);
		if (geoip_v6 != nullptr)
			GeoIP_delete(geoip_v6);
	};

	if (options.geoip) {
		geoip_v4 = GeoIP_open_type(GEOIP_COUNTRY_EDITION,
					   GEOIP_MEMORY_CACHE);
		if (geoip_v4 == nullptr)
			throw std::runtime_error("Failed to open GeoIP country IPv4 database - did you install package geoip-database?");

		GeoIP_set_charset(geoip_v4, GEOIP_CHARSET_UTF8);

		geoip_v6 = GeoIP_open_type(GEOIP_COUNTRY_EDITION_V6,
					   GEOIP_MEMORY_CACHE);
		if (geoip_v6 == nullptr)
			throw std::runtime_error("Failed to open GeoIP country IPv6 database - did you install package geoip-database?");

		GeoIP_set_charset(geoip_v6, GEOIP_CHARSET_UTF8);
	}


	ResultWriter result_writer(options.raw, options.gzip,
				   geoip_v4, geoip_v6,
				   options.anonymize,
				   options.track_visitors,
				   single_site,
				   options.per_site,
				   options.per_site_filename,
				   options.per_site_nested);

	if (filter.since != Net::Log::TimePoint::min())
		client.Send(id, PondRequestCommand::FILTER_SINCE, filter.since);

	if (filter.until != Net::Log::TimePoint::max())
		client.Send(id, PondRequestCommand::FILTER_UNTIL, filter.until);

	if (group_site.max_sites != 0)
		client.SendT(id, PondRequestCommand::GROUP_SITE, group_site);

	if (window.max != 0)
		client.SendT(id, PondRequestCommand::WINDOW, window);

	if (options.follow)
		client.Send(id, PondRequestCommand::FOLLOW);

	client.Send(id, PondRequestCommand::COMMIT);

	struct pollfd pfds[] = {
		/* waiting for messages from the Pond server */
		MakePollfd(client.GetSocket().ToFileDescriptor(), POLLIN),
		MakePollfd(result_writer.GetFileDescriptor(),
			   /* only waiting for POLLERR, which is an
			      output-only flag */
			   0),
	};

	while (true) {
		if (client.IsEmpty()) {
			/* if there is data in the buffer, flush it
			   after 100ms */
			int timeout = result_writer.IsEmpty() ? -1 : 100;

			int result = poll(pfds, std::size(pfds), timeout);
			if (result < 0)
				throw MakeErrno("poll() failed");

			if (result == 0) {
				/* no new data after 100ms: flush the
				   buffer and keep on waiting */
				result_writer.Flush();
				continue;
			}

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
			result_writer.Finish();
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

	result_writer.Finish();
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

static void
Cancel(const PondServerSpecification &server, ConstBuffer<const char *> args)
{
	if (!args.empty())
		throw "Bad arguments";

	PondClient client(PondConnect(server));
	const auto id = client.MakeId();
	client.Send(id, PondRequestCommand::CANCEL_OPERATION);
}

int
main(int argc, char **argv)
try {
	ConstBuffer<const char *> args(argv + 1, argc - 1);
	if (args.size < 2) {
		fprintf(stderr, "Usage: %s SERVER[:PORT] COMMAND ...\n"
			"\n"
			"Commands:\n"
			"  query\n"
			"    [--follow]\n"
			"    [--raw] [--gzip]\n"
			"    [--geoip] [--anonymize]\n"
			"    [--per-site=PATH] [--per-site-file=FILENAME] [--per-site-nested]\n"
			"    [type=http_access|http_error|submission] [site=VALUE] [group_site=[MAX][@SKIP]]\n"
			"    [since=ISO8601] [until=ISO8601] [date=YYYY-MM-DD] [today]\n"
			"  stats\n"
			"  inject <RAWFILE\n"
			"  clone OTHERSERVER[:PORT]\n"
			"  cancel\n",
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
	} else if (StringIsEqual(command, "cancel")) {
		Cancel(server, args);
		return EXIT_SUCCESS;
	} else {
		fprintf(stderr, "Unknown command: %s\n", command);
		return EXIT_FAILURE;
	}
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
