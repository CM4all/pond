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

#include <stdint.h>

enum class PondRequestCommand : uint16_t {
	NOP = 0,

	/**
	 * Commit the current request parameters and start executing
	 * the request.
	 */
	COMMIT = 1,

	/**
	 * Cancel this request.
	 */
	CANCEL = 2,

	/**
	 * Query records.  This packet initiates a new request.
	 */
	QUERY = 3,

	/**
	 * Specify a filter on the "site" attribute.  Payload is the
	 * exact string to compare with.
	 */
	FILTER_SITE = 4,

	/**
	 * Option for #QUERY which follows incoming new records
	 * instead of printing past ones.  The response never ends
	 * until the client sends #CANCEL (or closes the connection).
	 */
	FOLLOW = 5,

	/**
	 * Specify a filter on the "timestamp" attribute.  Payload is
	 * a 64 bit time stamp (microseconds since epoch).
	 */
	FILTER_SINCE = 6,

	/**
	 * Specify a filter on the "timestamp" attribute.  Payload is
	 * a 64 bit time stamp (microseconds since epoch).
	 */
	FILTER_UNTIL = 7,

	/**
	 * Option for #QUERY which limits the number of distinct sites
	 * in the response.  This is useful to write site-specific log
	 * files while keeping only a certain number of files open.
	 * Payload is #PondGroupSitePayload.
	 */
	GROUP_SITE = 8,

	/**
	 * Clear the local database and replace it with contents from
	 * another Pond server.  Payload is a string specifying the
	 * (numeric) address of the other Pond server.
	 */
	CLONE = 9,

	/**
	 * Specify a filter on the "type" attribute.  Payload is a
	 * #Net::Log::Type.
	 */
	FILTER_TYPE = 10,

	/**
	 * Inject a log record into the server database, as if it had
	 * been received on the UDP receiver.  Payload is the same as
	 * PondResponseCommand::LOG_RECORD.  Only accepted from
	 * privileged local clients.
	 */
	INJECT_LOG_RECORD = 11,

	/**
	 * Request statistics.  Returns #PondResponseCommand::STATS.
	 */
	STATS = 12,
};

enum class PondResponseCommand : uint16_t {
	NOP = 0,

	/**
	 * An error has occurred.  Payload is a human-readable error
	 * message.
	 */
	ERROR = 1,

	/**
	 * End of the current response.  Needed for some types of
	 * responses.
	 */
	END = 2,

	/**
	 * A serialized log record according to net/log/Protocol.hxx
	 */
	LOG_RECORD = 3,

	/**
	 * Statistics.  Response for #PondRequestCommand::STATS.
	 * Payload is #PondStatsPayload.
	 */
	STATS = 4,
};

/**
 * Everything is network byte order (big-endian).
 */
struct PondHeader {
	uint16_t id;
	uint16_t command;
	uint16_t size;
};

static_assert(sizeof(PondHeader) == 6, "Wrong size");

/**
 * Payload for PondRequestCommand::GROUP_SITE.
 */
struct PondGroupSitePayload {
	/**
	 * How many sites will be sent with this query?
	 */
	uint32_t max_sites;

	/**
	 * How many sites will be skipped with this query?
	 */
	uint32_t skip_sites;
};

/**
 * Payload for PondResponseCommand::STATS.
 */
struct PondStatsPayload {
	uint64_t memory_capacity, memory_usage;

	uint64_t n_records;

	/**
	 * The total number of datagrams received.  This includes
	 * discarded and malformed ones.
	 */
	uint64_t n_received;

	/**
	 * The number of malformed datagrams.
	 */
	uint64_t n_malformed;

	/**
	 * The number of discarded datagrams (e.g. due to rate
	 * limits).
	 */
	uint64_t n_discarded;
};
