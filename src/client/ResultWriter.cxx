// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ResultWriter.hxx"
#include "Protocol.hxx"
#include "FormatJson.hxx"
#include "system/Error.hxx"
#include "lib/zlib/GzipOutputStream.hxx"
#include "io/FdOutputStream.hxx"
#include "io/Iovec.hxx"
#include "io/Open.hxx"
#include "net/SendMessage.hxx"
#include "net/log/Datagram.hxx"
#include "net/log/Parser.hxx"
#include "util/ByteOrder.hxx"
#include "util/CharUtil.hxx"
#include "util/Macros.hxx"
#include "util/SpanCast.hxx"

#ifdef HAVE_AVAHI
#include "net/HostParser.hxx"
#include "util/StringSplit.hxx"
#endif

#include <algorithm>

#include <fcntl.h>
#include <string.h> // for strlen(), stpcpy()
#include <sys/stat.h> // for umask(), fchmod()

/**
 * Drop all entries of this file from the page cache.  This avoids
 * cluttering the page cache with data we'll never need again.
 */
static void
DropPageCache(FileDescriptor fd) noexcept
{
	const auto size = fd.GetSize();
	if (size > 0)
		posix_fadvise(fd.Get(), 0, size, POSIX_FADV_DONTNEED);
}

/**
 * Cast this #FileDescriptor to a #SocketDescriptor if it specifies a
 * socket.
 */
[[gnu::pure]]
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
[[gnu::pure]]
static SocketDescriptor
CheckPacketSocket(FileDescriptor fd)
{
	auto s = CheckSocket(fd);
	if (s.IsDefined() && s.IsStream())
		s.SetUndefined();
	return s;
}

static constexpr bool
IsSafeSiteChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch);
}

static const char *
SanitizeSiteName(char *buffer, size_t buffer_size,
		 const char *site)
{
	const size_t site_length = strlen(site);
	if (site_length == 0 || site_length >= buffer_size)
		return nullptr;

	bool ok = false;

	char *p = buffer;
	while (*site != 0) {
		char ch = *site++;

		if (IsSafeSiteChar(ch))
			ok = true;
		else
			ch = '_';

		*p++ = ch;
	}

	if (!ok)
		return nullptr;

	*p = 0;
	return buffer;
}

static void
SendPacket(SocketDescriptor s, std::span<const std::byte> payload)
{
	const struct iovec vec[] = {
		MakeIovec(payload),
	};

	SendMessage(s, MessageHeader{std::span{vec}}, 0);
}

ResultWriter::ResultWriter(bool _raw, bool _gzip,
#ifdef HAVE_LIBGEOIP
			   GeoIP *_geoip_v4, GeoIP *_geoip_v6,
#endif
			   bool _track_visitors,
#ifdef HAVE_AVAHI
			   bool _resolve_forwarded_to,
#endif
			   const Net::Log::OneLineOptions _one_line_options,
			   bool _jsonl,
			   bool _single_site,
			   const char *const _per_site,
			   const char *const _per_site_filename,
			   bool _per_site_nested)
	:fd(STDOUT_FILENO),
	 socket(CheckPacketSocket(fd)),
#ifdef HAVE_LIBGEOIP
	 geoip_v4(_geoip_v4), geoip_v6(_geoip_v6),
#endif
	 per_site(_per_site, _per_site_filename, _per_site_nested),
	 one_line_options(_one_line_options),
#ifdef HAVE_AVAHI
	 resolve_forwarded_to(_resolve_forwarded_to),
#endif
	 jsonl(_jsonl),
	 raw(_raw), gzip(_gzip),
	 track_visitors(_track_visitors)
{
	if (per_site.IsDefined()) {
		one_line_options.show_site = false;

		fd.SetUndefined();
		socket.SetUndefined();

		const auto u = umask(0222);
		umask(u);

		file_mode = 0666 & ~u;
	} else {
		one_line_options.show_site = !_single_site;

		fd_output_stream = std::make_unique<FdOutputStream>(fd);
		output_stream = fd_output_stream.get();

		if (gzip) {
			gzip_output_stream = std::make_unique<GzipOutputStream>(*output_stream);
			output_stream = gzip_output_stream.get();
		}
	}
}

ResultWriter::~ResultWriter() noexcept = default;

#ifdef HAVE_LIBGEOIP

inline const char *
ResultWriter::LookupGeoIP(const char *address) const noexcept
{
	const char *dot = strchr(address, '.');
	if (dot != nullptr)
		return GeoIP_country_code_by_addr(geoip_v4, address);

	const char *colon = strchr(address, ':');
	if (colon != nullptr)
		return GeoIP_country_code_by_addr_v6(geoip_v6, address);

	return nullptr;
}

#endif // HAVE_LIBGEOIP

void
ResultWriter::Append(Net::Log::Datagram &&d)
{
#ifdef HAVE_AVAHI
	if (resolve_forwarded_to && d.forwarded_to != nullptr) {
		/* extract the IP address, stripping the port and
		   square brackets (IPv6) */
		if (const auto e = ExtractHost(d.forwarded_to); !e.host.empty())
			/* remove the scope name (IPv6) because Avahi
			   doesn't understand it and query the Avahi
			   address resolver */
			if (const char *name = address_resolver.ResolveAddress(std::string{Split(e.host, '%').first}))
				d.forwarded_to = name;
	}
#endif // HAVE_AVAHI

	if (buffer_fill > sizeof(buffer) - 16384)
		FlushBuffer();

	char *old_end = buffer + buffer_fill;

	if (jsonl) {
		char *end = FormatJson(old_end, buffer + sizeof(buffer) - 1, d);
		if (end == old_end)
			return;

		*end++ = '\n';
		buffer_fill = end - buffer;
		return;
	}

	char *end = Net::Log::FormatOneLine(old_end,
					    sizeof(buffer) - buffer_fill - 64,
					    d, one_line_options);
	if (end == old_end)
		return;

#ifdef HAVE_LIBGEOIP
	if (d.IsHttpAccess() && geoip_v4 != nullptr) {
		const char *country = d.remote_host != nullptr
			? LookupGeoIP(d.remote_host)
			: nullptr;
		if (country == nullptr)
			country = "-";

		*end++ = ' ';
		end = stpcpy(end, country);
	}
#endif // HAVE_LIBGEOIP

	if (d.IsHttpAccess() && track_visitors) {
		const char *visitor_id =
			d.remote_host != nullptr && d.HasTimestamp()
			? visitor_tracker.MakeVisitorId(d.remote_host,
							d.timestamp)
			: "-";

		*end++ = ' ';
		end = stpcpy(end, visitor_id);
	}

	*end++ = '\n';
	buffer_fill = end - buffer;
}

void
ResultWriter::Write(std::span<const std::byte> payload)
{
	if (per_site.IsDefined()) {
		auto d = Net::Log::ParseDatagram(payload);
		if (d.site == nullptr)
			// TODO: where to log datagrams without a site?
			return;

		if (!d.IsHttpAccess() && d.message.data() == nullptr)
			/* this is neither a HTTP access nor does it
			   contain a message - FormatOneLine() will
			   not generate anything, so don't bother
			   opening the output file */
			return;

		char filename_buffer[sizeof(last_site)];
		const char *filename = SanitizeSiteName(filename_buffer,
							sizeof(filename_buffer),
							d.site);
		if (filename == nullptr)
			return;

		if (strcmp(last_site, filename) != 0) {
			if (per_site_fd.IsDefined()) {
				/* flush data belonging into the currently
				   open output file */
				Finish();
			}

			per_site_fd = per_site.Open(filename);
			if (!per_site_fd.IsDefined())
				/* skip this site */
				return;

			if (file_mode >= 0)
				/* work around a Linux kernel bug
				   which fails to apply the umask when
				   O_TMPFILE is used */
				fchmod(per_site_fd.GetFileDescriptor().Get(),
				       file_mode);

			fd_output_stream = std::make_unique<FdOutputStream>(per_site_fd.GetFileDescriptor());
			output_stream = fd_output_stream.get();

			if (gzip) {
				gzip_output_stream = std::make_unique<GzipOutputStream>(*output_stream);
				output_stream = gzip_output_stream.get();
			}

			strcpy(last_site, filename);

			/* visitor ids are unique within the output
			   file, so a new site output file gets new
			   ids */
			visitor_tracker.Reset();
		} else if (!per_site_fd.IsDefined())
			/* skip this site */
			return;

		Append(std::move(d));
	} else if (socket.IsDefined()) {
		/* if fd2 is a packet socket, send raw
		   datagrams to it */
		SendPacket(socket, payload);
	} else if (raw) {
		const uint16_t id = 1;
		const auto command = PondResponseCommand::LOG_RECORD;
		const PondHeader header{ToBE16(id), ToBE16(uint16_t(command)), ToBE16(payload.size())};
		output_stream->Write(ReferenceAsBytes(header));
		output_stream->Write(payload);
	} else
		Append(Net::Log::ParseDatagram(payload));
}

void
ResultWriter::FlushBuffer()
{
	if (buffer_fill == 0)
		return;

	assert(output_stream);

	output_stream->Write(std::as_bytes(std::span{buffer}.first(buffer_fill)));
	buffer_fill = 0;
}

void
ResultWriter::Flush()
{
	FlushBuffer();
}

void
ResultWriter::Finish()
{
	Flush();

	if (gzip_output_stream) {
		/* doing a Z_SYNC_FLUSH now to align the last block on
		   a byte boundary, which allows simple concatenation
		   of gzipped files without having to decompress (and
		   pad) them */
		gzip_output_stream->SyncFlush();

		gzip_output_stream->Finish();
		gzip_output_stream.reset();
	}

	fd_output_stream.reset();

	if (per_site_fd.IsDefined()) {
		/* if we're writing one file per site, most likely we
		   won't need to read those files soon, let's avoid
		   cluttering the page cache by dropping those
		   pages */
		DropPageCache(per_site_fd.GetFileDescriptor());

		per_site_fd.Commit();
	}
}
