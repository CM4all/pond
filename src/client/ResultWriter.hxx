// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "PerSitePath.hxx"
#include "VisitorTracker.hxx"
#include "io/FileDescriptor.hxx"
#include "io/FileWriter.hxx"
#include "net/SocketDescriptor.hxx"
#include "net/log/OneLine.hxx"
#include "config.h"

#ifdef HAVE_LIBGEOIP
#include <GeoIP.h>
#endif

#include <memory>
#include <span>

namespace Net::Log { struct Datagram; }
class OutputStream;
class FdOutputStream;
class GzipOutputStream;

class ResultWriter {
	FileDescriptor fd;
	SocketDescriptor socket;

#ifdef HAVE_LIBGEOIP
	GeoIP *const geoip_v4, *const geoip_v6;
#endif

	OutputStream *output_stream;
	std::unique_ptr<FdOutputStream> fd_output_stream;
	std::unique_ptr<GzipOutputStream> gzip_output_stream;

	PerSitePath per_site;
	char last_site[256] = "";
	FileWriter per_site_fd;

	/**
	 * The mode to be applied to new files; this is a workaround
	 * for buggy Linux kernels which fail to apply the umask when
	 * O_TMPFILE is used.
	 */
	int file_mode = -1;

	VisitorTracker visitor_tracker;

	Net::Log::OneLineOptions one_line_options;

	const bool jsonl;

	const bool raw, gzip, track_visitors;

	size_t buffer_fill = 0;
	char buffer[65536];

public:
	ResultWriter(bool _raw, bool _gzip,
#ifdef HAVE_LIBGEOIP
		     GeoIP *_geoip_v4, GeoIP *_geoip_v6,
#endif
		     bool _track_visitors,
		     Net::Log::OneLineOptions _one_line_options,
		     bool _jsonl,
		     bool _single_site,
		     const char *const _per_site,
		     const char *const _per_site_filename,
		     bool _per_site_nested);
	~ResultWriter() noexcept;

	ResultWriter(const ResultWriter &) = delete;
	ResultWriter &operator=(const ResultWriter &) = delete;

	FileDescriptor GetFileDescriptor() const noexcept {
		return fd;
	}

	bool IsEmpty() const noexcept {
		return buffer_fill == 0;
	}

	void Write(std::span<const std::byte> payload);

	/**
	 * Flushes pending data.
	 */
	void Flush();

	/**
	 * Finish the current file and commit it to disk.  No more
	 * data may be written after that.
	 */
	void Finish();

private:
	void FlushBuffer();

#ifdef HAVE_LIBGEOIP
	[[gnu::pure]]
	const char *LookupGeoIP(const char *address) const noexcept;
#endif

	void Append(const Net::Log::Datagram &d);
};
