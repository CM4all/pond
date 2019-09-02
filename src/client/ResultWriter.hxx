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

#pragma once

#include "PerSitePath.hxx"
#include "io/FileDescriptor.hxx"
#include "io/FileWriter.hxx"
#include "net/SocketDescriptor.hxx"

#include <GeoIP.h>

#include <memory>

template<typename t> struct ConstBuffer;
namespace Net::Log { struct Datagram; }
class OutputStream;
class FdOutputStream;
class GzipOutputStream;

class ResultWriter {
	FileDescriptor fd;
	SocketDescriptor socket;

	GeoIP *const geoip_v4, *const geoip_v6;

	OutputStream *output_stream;
	std::unique_ptr<FdOutputStream> fd_output_stream;
	std::unique_ptr<GzipOutputStream> gzip_output_stream;

	PerSitePath per_site;
	char last_site[256] = "";
	FileWriter per_site_fd;

	const bool raw, gzip, anonymize, single_site;

	size_t buffer_fill = 0;
	char buffer[65536];

public:
	ResultWriter(bool _raw, bool _gzip,
		     GeoIP *_geoip_v4, GeoIP *_geoip_v6, bool _anonymize,
		     bool _single_site,
		     const char *const _per_site,
		     const char *const _per_site_filename) noexcept;
	~ResultWriter() noexcept;

	ResultWriter(const ResultWriter &) = delete;
	ResultWriter &operator=(const ResultWriter &) = delete;

	FileDescriptor GetFileDescriptor() const noexcept {
		return fd;
	}

	bool IsEmpty() const noexcept {
		return buffer_fill == 0;
	}

	void Write(ConstBuffer<void> payload);

	void Flush();

private:
	void FlushBuffer();

	gcc_pure
	const char *LookupGeoIP(const char *address) const noexcept;

	void Append(const Net::Log::Datagram &d, bool site);
};
