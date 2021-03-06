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

#pragma once

#include "SmallDatagram.hxx"
#include "util/ConstBuffer.hxx"

#include <boost/intrusive/list_hook.hpp>

class Record final {
public:
	using ListHook =
		boost::intrusive::list_member_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>;
	ListHook per_site_list_hook;

private:
	uint64_t id;

	const size_t raw_size;

	SmallDatagram parsed;

public:
	/**
	 * Throws Net::Log::ProtocolError on error.
	 */
	Record(uint64_t _id, ConstBuffer<uint8_t> _raw);

	Record(const Record &) = delete;
	Record &operator=(const Record &) = delete;

	uint64_t GetId() const noexcept {
		return id;
	}

	ConstBuffer<void> GetRaw() const noexcept {
		return {this + 1, raw_size};
	}

	const auto &GetParsed() const noexcept {
		return parsed;
	}

	bool IsOlderThan(Net::Log::TimePoint t) const noexcept {
		return parsed.HasTimestamp() && parsed.timestamp < t;
	}

	/**
	 * Like IsOlderThan(), but also return true if the time stamp
	 * is not known.  Used by Database::DeleteOlderThan().
	 */
	bool IsOlderThanOrUnknown(Net::Log::TimePoint t) const noexcept {
		return !parsed.HasTimestamp() || parsed.timestamp < t;
	}
};
