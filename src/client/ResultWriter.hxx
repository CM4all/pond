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

#ifdef HAVE_AVAHI
#include "CachedAddressResolver.hxx"
#endif

#ifdef HAVE_LIBGEOIP
#include <GeoIP.h>
#endif

#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>

namespace Net::Log { struct Datagram; }
class OutputStream;
class FdOutputStream;
class GzipOutputStream;

struct AccumulateParams {
	bool enabled = false;

	enum class Field : uint_least8_t {
		REMOTE_HOST,
		HOST,
		SITE,
	} field;

	enum class Type : uint_least8_t {
		TOP,
		MORE,
	} type;

	std::size_t count;
};

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

	AccumulateParams accumulate_params;
	std::unordered_map<std::string, std::size_t> accumulate_map;

#ifdef HAVE_AVAHI
	CachedAddressResolver address_resolver;

	const bool resolve_forwarded_to;
#endif // HAVE_AVAHI

	const bool jsonl;

	const bool raw, age_only, gzip, track_visitors;

	size_t buffer_fill = 0;
	char buffer[65536];

public:
	ResultWriter(bool _raw, bool _age_only, bool _gzip,
#ifdef HAVE_LIBGEOIP
		     GeoIP *_geoip_v4, GeoIP *_geoip_v6,
#endif
		     bool _track_visitors,
#ifdef HAVE_AVAHI
		     bool _resolve_forwarded_to,
#endif
		     Net::Log::OneLineOptions _one_line_options,
		     bool _jsonl,
		     AccumulateParams _accumulate_params,
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
	void PrintAccumulateTop();
	void PrintAccumulateMore();
	void PrintAccumulate();

	void FlushBuffer();

#ifdef HAVE_LIBGEOIP
	[[gnu::pure]]
	const char *LookupGeoIP(const char *address) const noexcept;
#endif

	void Append(Net::Log::Datagram &&d);
};
