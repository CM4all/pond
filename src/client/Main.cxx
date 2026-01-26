// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Protocol.hxx"
#include "Datagram.hxx"
#include "Client.hxx"
#include "ResultWriter.hxx"
#include "Open.hxx"
#include "Filter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "system/Error.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/log/String.hxx"
#include "net/log/Parser.hxx"
#include "time/Parser.hxx"
#include "time/Math.hxx"
#include "time/Convert.hxx"
#include "util/PrintException.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "util/ByteOrder.hxx"
#include "util/NumberParser.hxx"
#include "util/ScopeExit.hxx"
#include "util/StaticFifoBuffer.hxx"
#include "util/StringSplit.hxx"
#include "config.h"

#ifdef HAVE_AVAHI
#include "lib/avahi/Check.hxx"
#endif

#include <fmt/core.h>

#include <concepts>
#include <span>

#include <stdlib.h>
#include <poll.h>

using std::string_view_literals::operator""sv;

[[gnu::pure]]
static const char *
IsFilter(const char *arg, std::string_view name) noexcept
{
	return StringStartsWith(arg, name) && arg[name.size()] == '='
		? arg + name.size() + 1
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

	Net::Log::OneLineOptions one_line;

	AccumulateParams accumulate;

	bool jsonl = false;

	bool follow = false, continue_ = false;
	bool last = false;
	bool age_only = false;
	bool raw = false;
	bool gzip = false;
#ifdef HAVE_LIBGEOIP
	bool geoip = false;
#endif
	bool track_visitors = false;
	bool per_site_nested = false;

#ifdef HAVE_AVAHI
	bool resolve_forwarded_to = false;
#endif
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
	} else if (auto host = IsFilter(p, "host")) {
		if (!filter.hosts.emplace(host).second)
			throw "Duplicate host name";
	} else if (auto generator = IsFilter(p, "generator")) {
		if (!filter.generators.emplace(generator).second)
			throw "Duplicate generator name";
	} else if (auto since = IsFilter(p, "since")) {
		auto t = ParseTimePoint(since);
		filter.timestamp.since = Net::Log::FromSystem(t.first);
	} else if (auto until = IsFilter(p, "until")) {
		auto t = ParseTimePoint(until);
		filter.timestamp.until = Net::Log::FromSystem(t.first + t.second);
	} else if (auto time = IsFilter(p, "time")) {
		auto t = ParseTimePoint(time);
		filter.timestamp.since = Net::Log::FromSystem(t.first);
		filter.timestamp.until = Net::Log::FromSystem(t.first + t.second);
	} else if (auto date_string = IsFilter(p, "date")) {
		const auto date = ParseLocalDate(date_string);
		filter.timestamp.since = Net::Log::FromSystem(date);
		filter.timestamp.until = Net::Log::FromSystem(date + std::chrono::hours(24));
	} else if (StringIsEqual(p, "today")) {
		const auto midnight =
			PrecedingMidnightLocal(std::chrono::system_clock::now());
		filter.timestamp.since = Net::Log::FromSystem(midnight);
		filter.timestamp.until = Net::Log::FromSystem(midnight + std::chrono::hours(24));
	} else if (auto duration_longer = IsFilter(p, "duration_longer")) {
		auto d = ParseDuration(duration_longer);
		filter.duration.longer = std::chrono::duration_cast<Net::Log::Duration>(d.first);
	} else if (auto type_string = IsFilter(p, "type")) {
		filter.type = Net::Log::ParseType(type_string);
		if (filter.type == Net::Log::Type::UNSPECIFIED)
			throw "Bad type filter";
	} else if (auto status_string = IsFilter(p, "status")) {
		char *endptr;
		long begin = strtol(status_string, &endptr, 10);
		if (endptr == status_string || begin < 0 || begin >= 600)
			throw "Bad status filter";

		long end = begin + 1;

		if (*endptr == ':') {
			status_string = endptr + 1;
			end = strtol(status_string, &endptr, 10);
			if (endptr == status_string || *endptr != 0 || end <= begin || end > 600)
				throw "Bad status filter";
		} else if (*endptr != 0)
			throw "Bad status filter";

		filter.http_status.begin = begin;
		filter.http_status.end = end;
	} else if (StringIsEqual(p, "unsafe_method")) {
		filter.http_method_unsafe = true;
	} else if (auto uri_prefix = IsFilter(p, "uri-prefix")) {
		if (*uri_prefix == 0)
			throw "Bad URI prefix";

		filter.http_uri_starts_with = uri_prefix;
	} else if (auto per_site = StringAfterPrefix(p, "--per-site=")) {
		options.per_site = per_site;
	} else if (auto per_site_filename = StringAfterPrefix(p, "--per-site-file=")) {
		if (options.per_site == nullptr)
			throw "--per-site-file requires --per-site";
		options.per_site_filename = per_site_filename;
	} else if (StringIsEqual(p, "--per-site-nested")) {
		options.per_site_nested = true;
	} else if (StringIsEqual(p, "--follow")) {
		if (options.continue_)
			throw "Cannot use both --follow and --continue";

		options.follow = true;
	} else if (StringIsEqual(p, "--continue")) {
		if (options.follow)
			throw "Cannot use both --follow and --continue";

		options.continue_ = true;
	} else if (StringIsEqual(p, "--last")) {
		options.last = true;
	} else if (StringIsEqual(p, "--age-only")) {
		options.age_only = true;
	} else if (StringIsEqual(p, "--raw"))
		options.raw = true;
	else if (StringIsEqual(p, "--gzip"))
		options.gzip = true;
#ifdef HAVE_LIBGEOIP
	else if (StringIsEqual(p, "--geoip"))
		options.geoip = true;
#endif
	else if (StringIsEqual(p, "--anonymize"))
		options.one_line.anonymize = true;
	else if (StringIsEqual(p, "--track-visitors"))
		options.track_visitors = true;
	else if (StringIsEqual(p, "--host"))
		options.one_line.show_host = true;
	else if (StringIsEqual(p, "--forwarded-to")) {
		options.one_line.show_forwarded_to = true;
#ifdef HAVE_AVAHI
	} else if (StringIsEqual(p, "--resolve-forwarded-to")) {
		options.one_line.show_forwarded_to = true;
		options.resolve_forwarded_to = true;
#endif
	} else if (StringIsEqual(p, "--no-referer"))
		options.one_line.show_http_referer = false;
	else if (StringIsEqual(p, "--no-agent"))
		options.one_line.show_user_agent = false;
	else if (StringIsEqual(p, "--content-type"))
		options.one_line.show_content_type = true;
	else if (StringIsEqual(p, "--iso8601"))
		options.one_line.iso8601 = true;
	else if (StringIsEqual(p, "--jsonl"))
		options.jsonl = true;
	else if (const char *accumulate = StringAfterPrefix(p, "--accumulate=")) {
		const auto [field, rest1] = Split(std::string_view{accumulate}, ',');

		if (field == "remote_host"sv)
			options.accumulate.field = AccumulateParams::Field::REMOTE_HOST;
		else if (field == "host"sv)
			options.accumulate.field = AccumulateParams::Field::HOST;
		else if (field == "site"sv)
			options.accumulate.field = AccumulateParams::Field::SITE;
		else
			throw "Unrecognized field";

		const auto [type, count] = Split(rest1, ',');
		if (type == "top"sv)
			options.accumulate.type = AccumulateParams::Type::TOP;
		else if (type == "more"sv)
			options.accumulate.type = AccumulateParams::Type::MORE;
		else
			throw "Unrecognized type";

		if (!ParseIntegerTo(count, options.accumulate.count))
			throw "Invalid number";

		options.accumulate.enabled = true;
	} else
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
Query(const PondServerSpecification &server, std::span<const char *const> args)
{
	Filter filter;
	PondGroupSitePayload group_site{0, 0};
	PondWindowPayload window{};
	QueryOptions options;

	while (!args.empty()) {
		const char *p = args.front();
		args = args.subspan(1);
		try {
			ParseFilterItem(filter, group_site, window,
					options, p);
		} catch (...) {
			std::throw_with_nested(FmtRuntimeError("Failed to parse {:?}", p));
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

	for (const auto &i : filter.hosts)
		client.Send(id, PondRequestCommand::FILTER_HOST, i);

	for (const auto &i : filter.generators)
		client.Send(id, PondRequestCommand::FILTER_GENERATOR, i);

	const bool single_site = filter.sites.begin() != filter.sites.end() &&
		std::next(filter.sites.begin()) == filter.sites.end();

#ifdef HAVE_LIBGEOIP
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
#endif // HAVE_LIBGEOIP

	ResultWriter result_writer{
		options.raw,
		options.age_only,
		options.gzip,
#ifdef HAVE_LIBGEOIP
		geoip_v4, geoip_v6,
#endif
		options.track_visitors,
#ifdef HAVE_AVAHI
		options.resolve_forwarded_to,
#endif
		options.one_line,
		options.jsonl,
		options.accumulate,
		single_site,
		options.per_site,
		options.per_site_filename,
		options.per_site_nested,
	};

	if (filter.timestamp.HasSince())
		client.Send(id, PondRequestCommand::FILTER_SINCE, filter.timestamp.since);

	if (filter.timestamp.HasUntil())
		client.Send(id, PondRequestCommand::FILTER_UNTIL, filter.timestamp.until);

	if (filter.duration.HasLonger())
		client.Send(id, PondRequestCommand::FILTER_DURATION_LONGER,
			    filter.duration.longer);

	if (filter.http_status) {
		PondFilterHttpStatusPayload status;
		status.begin = ToBE16(filter.http_status.begin);
		status.end = ToBE16(filter.http_status.end);
		client.SendT(id, PondRequestCommand::FILTER_HTTP_STATUS, status);
	}

	if (filter.http_method_unsafe)
		client.Send(id, PondRequestCommand::FILTER_HTTP_METHOD_UNSAFE);

	if (!filter.http_uri_starts_with.empty())
		client.Send(id, PondRequestCommand::FILTER_HTTP_URI_STARTS_WITH, filter.http_uri_starts_with);

	if (group_site.max_sites != 0)
		client.SendT(id, PondRequestCommand::GROUP_SITE, group_site);

	if (window.max != 0)
		client.SendT(id, PondRequestCommand::WINDOW, window);

	if (options.follow)
		client.Send(id, PondRequestCommand::FOLLOW);

	if (options.continue_)
		client.Send(id, PondRequestCommand::CONTINUE);

	if (options.last)
		client.Send(id, PondRequestCommand::LAST);

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
			throw FmtRuntimeError("Server error: {}", d.payload.ToString());

		case PondResponseCommand::END:
			result_writer.Finish();
			return;

		case PondResponseCommand::LOG_RECORD:
			try {
				result_writer.Write(d.payload);
			} catch (Net::Log::ProtocolError) {
				fmt::print(stderr, "Failed to parse log record\n");
			}

			break;

		case PondResponseCommand::STATS:
			throw "Unexpected response packet";
		}
	}

	result_writer.Finish();
}

static void
Stats(const PondServerSpecification &server, std::span<const char *const> args)
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

	std::span<const std::byte> payload = response.payload;
	const PondStatsPayload &stats = *(const PondStatsPayload *)
		(const void *)payload.data();

	if (payload.size() < sizeof(PondStatsPayload))
		// TODO: backwards compatibility, allow smaller payloads
		throw "Wrong response payload size";

	fmt::print("memory_capacity={}\n"
		   "memory_usage={}\n"
		   "n_records={}\n",
		   FromBE64(stats.memory_capacity),
		   FromBE64(stats.memory_usage),
		   FromBE64(stats.n_records));

	fmt::print("n_received={}\n"
		   "n_malformed={}\n"
		   "n_discarded={}\n",
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
		throw SocketBufferFullError{};

	auto nbytes = fd.Read(w);
	if (nbytes < 0)
		throw MakeErrno("Failed to read");

	buffer.Append(nbytes);
	return nbytes;
}

static void
ReadPackets(FileDescriptor fd,
	    std::invocable<unsigned, unsigned, std::span<const std::byte>> auto f)
{
	StaticFifoBuffer<std::byte, 65536> input;

	while (true) {
		auto r = input.Read();
		// TODO: alignment/padding?
		const auto &header = *(const PondHeader *)(const void *)r.data();
		if (r.size() < sizeof(header) ||
		    r.size() < sizeof(header) + FromBE16(header.size)) {
			size_t nbytes = ReadToBuffer(fd, input);
			if (nbytes == 0) {
				if (!input.empty())
					throw SocketGarbageReceivedError{"Trailing garbage"};
				return;
			}

			continue;
		}

		f(FromBE16(header.id), FromBE16(header.command),
		  std::span{(const std::byte *)(&header + 1), FromBE16(header.size)});
		input.Consume(sizeof(header) + FromBE16(header.size));
	}


}

static void
Inject(const PondServerSpecification &server, std::span<const char *const> args)
{
	if (!args.empty())
		throw "Bad arguments";

	PondClient client(PondConnect(server));

	ReadPackets(FileDescriptor(STDIN_FILENO), [&client](unsigned, unsigned command, std::span<const std::byte> payload){
		if (command == unsigned(PondResponseCommand::LOG_RECORD))
			client.Send(client.MakeId(),
				    PondRequestCommand::INJECT_LOG_RECORD,
				    payload);
	});
}

static void
Clone(const PondServerSpecification &server, std::span<const char *const> args)
{
	if (args.size() != 1)
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
			throw FmtRuntimeError("Server error: {}",
					      d.payload.ToString());

		case PondResponseCommand::END:
			return;

		case PondResponseCommand::LOG_RECORD:
		case PondResponseCommand::STATS:
			throw "Unexpected response packet";
		}
	}
}

static void
Cancel(const PondServerSpecification &server, std::span<const char *const> args)
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
	std::span<const char *const> args{argv + 1, static_cast<std::size_t>(argc - 1)};
	if (args.size() < 2) {
		fmt::print("Usage: {} SERVER[:PORT] COMMAND ...\n"
			   "\n"
			   "Commands:\n"
			   "  query\n"
			   "    [--follow] [--continue]\n"
			   "    [--last]\n"
			   "    [--raw] [--gzip]\n"
#ifdef HAVE_LIBGEOIP
			   "    [--geoip]\n"
#endif // HAVE_LIBGEOIP
			   "    [--anonymize] [--track-visitors]\n"
			   "    [--accumulate=FIELD,{{top|more}},COUNT]\n"
			   "    [--per-site=PATH] [--per-site-file=FILENAME] [--per-site-nested]\n"
			   "    [--host] [--forwarded-to] [--no-referer] [--no-agent]\n"
			   "    [--content-type]\n"
#ifdef HAVE_AVAHI
			   "    [--resolve-forwarded-to]\n"
#endif
			   "    [--iso8601]\n"
			   "    [--jsonl]\n"
			   "    [type=http_access|http_error|submission|ssh|job|history]\n"
			   "    [site=VALUE] [group_site=[MAX][@SKIP]]\n"
			   "    [host=VALUE]\n"
			   "    [uri-prefix=VALUE]\n"
			   "    [status=STATUSCODE[:END]]\n"
			   "    [unsafe_method]\n"
			   "    [generator=VALUE]\n"
			   "    [since=ISO8601] [until=ISO8601] [date=YYYY-MM-DD] [today]\n"
			   "    [duration_longer=DURATION]\n"
			   "    [window=COUNT[@SKIP]]\n"
			   "  stats\n"
			   "  inject <RAWFILE\n"
			   "  clone OTHERSERVER[:PORT]\n"
			   "  cancel\n",
			   argv[0]);
		return EXIT_FAILURE;
	}

	PondServerSpecification server;
	server.host = args.front();
	args = args.subspan(1);
#ifdef HAVE_AVAHI
	if (auto zs = StringAfterPrefix(server.host, "zeroconf/"))
		server.zeroconf_service = MakeZeroconfServiceType(zs, "_tcp");
#endif // HAVE_AVAHI

	const char *const command = args.front();
	args = args.subspan(1);

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
		fmt::print("Unknown command: {}\n", command);
		return EXIT_FAILURE;
	}
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
