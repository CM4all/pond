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
#include "zlib/GzipOutputStream.hxx"
#include "io/FdOutputStream.hxx"
#include "io/Open.hxx"
#include "net/SendMessage.hxx"
#include "net/log/Datagram.hxx"
#include "net/log/OneLine.hxx"
#include "net/log/Parser.hxx"
#include "util/ByteOrder.hxx"
#include "util/CharUtil.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Macros.hxx"

#include <algorithm>

#include <fcntl.h>

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

	char *p = buffer;
	while (*site != 0) {
		char ch = *site++;
		*p++ = IsSafeSiteChar(ch) ? ch : '_';
	}

	*p = 0;
	return buffer;
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

ResultWriter::ResultWriter(bool _raw, bool _gzip,
			   GeoIP *_geoip_v4, GeoIP *_geoip_v6,
			   bool _anonymize,
			   bool _track_visitors,
			   bool _single_site,
			   const char *const _per_site,
			   const char *const _per_site_filename,
			   bool _per_site_nested)
	:fd(STDOUT_FILENO),
	 socket(CheckPacketSocket(fd)),
	 geoip_v4(_geoip_v4), geoip_v6(_geoip_v6),
	 per_site(_per_site, _per_site_filename, _per_site_nested),
	 raw(_raw), gzip(_gzip), anonymize(_anonymize),
	 track_visitors(_track_visitors),
	 single_site(_single_site)
{
	if (per_site.IsDefined()) {
		fd.SetUndefined();
		socket.SetUndefined();
	} else {
		fd_output_stream = std::make_unique<FdOutputStream>(fd);
		output_stream = fd_output_stream.get();

		if (gzip) {
			gzip_output_stream = std::make_unique<GzipOutputStream>(*output_stream);
			output_stream = gzip_output_stream.get();
		}
	}
}

ResultWriter::~ResultWriter() noexcept = default;

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

void
ResultWriter::Append(const Net::Log::Datagram &d, bool site)
{
	if (buffer_fill > sizeof(buffer) - 16384)
		FlushBuffer();

	char *old_end = buffer + buffer_fill;

	char *end = FormatOneLine(old_end,
				  sizeof(buffer) - buffer_fill - 64,
				  d, site, anonymize);
	if (end == old_end)
		return;

	if (d.GuessIsHttpAccess() && geoip_v4 != nullptr) {
		const char *country = d.remote_host != nullptr
			? LookupGeoIP(d.remote_host)
			: nullptr;
		if (country == nullptr)
			country = "-";

		*end++ = ' ';
		end = stpcpy(end, country);
	}

	if (d.GuessIsHttpAccess() && track_visitors) {
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
ResultWriter::Write(ConstBuffer<void> payload)
{
	if (per_site.IsDefined()) {
		const auto d = Net::Log::ParseDatagram(payload);
		if (d.site == nullptr)
			// TODO: where to log datagrams without a site?
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

		Append(d, false);
	} else if (socket.IsDefined()) {
		/* if fd2 is a packet socket, send raw
		   datagrams to it */
		SendPacket(socket, payload);
	} else if (raw) {
		const uint16_t id = 1;
		const auto command = PondResponseCommand::LOG_RECORD;
		const PondHeader header{ToBE16(id), ToBE16(uint16_t(command)), ToBE16(payload.size)};
		output_stream->Write(&header, sizeof(header));
		output_stream->Write(payload.data, payload.size);
	} else
		Append(Net::Log::ParseDatagram(payload), !single_site);
}

void
ResultWriter::FlushBuffer()
{
	if (buffer_fill == 0)
		return;

	assert(output_stream);

	output_stream->Write(buffer, buffer_fill);
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
		gzip_output_stream->Finish();
		gzip_output_stream.reset();
	}

	fd_output_stream.reset();

	if (per_site_fd.IsDefined())
		per_site_fd.Commit();
}
