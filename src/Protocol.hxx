// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
	 *
	 * This is similar to #CONTINUE, but only prints new records.
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
	 * Group all result records by their "site" attribute,
	 * i.e. all records with the same site will be returned
	 * successively, followed by all records of the next site and
	 * so on.  This is useful to write site-specific log files
	 * while keeping only a certain number of files open.  Payload
	 * is #PondGroupSitePayload.
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

	/**
	 * Select a portion (window) of the result.  Payload is
	 * #PondWindowPayload.
	 */
	WINDOW = 13,

	/**
	 * Cancel the currently running #BlockingOperation.
	 */
	CANCEL_OPERATION = 14,

	/**
	 * Specify a filter on the "http_status" attribute.  Payload
	 * is two 16 bit integers specifying a range of HTTP status
	 * codes.
	 */
	FILTER_HTTP_STATUS = 15,

	/**
	 * Specify a filter on the "http_uri" attribute.  Payload is a
	 * non-empty string the URI is expected to start with.
	 */
	FILTER_HTTP_URI_STARTS_WITH = 16,

	/**
	 * Specify a filter on the "host" attribute.  Payload is the
	 * exact string to compare with.
	 */
	FILTER_HOST = 17,

	/**
	 * Specify a filter on the "generator" attribute.  Payload is
	 * the exact string to compare with.
	 */
	FILTER_GENERATOR = 18,

	/**
	 * Specify a filter on the "duration" attribute.  Payload is
	 * a 64 bit unsigned integer [microseconds].
	 */
	FILTER_DURATION_LONGER = 19,

	/**
	 * Option for #QUERY which follows incoming new records after
	 * printing past ones.  The response never ends until the
	 * client sends #CANCEL (or closes the connection).
	 *
	 * This is similar to #FOLLOW, but also prints matching past
	 * records.
	 */
	CONTINUE = 20,

	/**
	 * Print only the last matching record.
	 */
	LAST = 21,

	/**
	 * Specify a filter on "unsafe" HTTP methods (according to RFC
	 * 2616 9.1.1 and RFC 9110 9.2.1).
	 */
	FILTER_HTTP_METHOD_UNSAFE = 22,

	/**
	 * Specify a filter on the HTTP method.  Payload is a 32 bit mask
	 * based on the #HttpMethod enum.
	 */
	FILTER_HTTP_METHODS = 23,

	/**
	 * Specify a filter on the exact value of the "http_uri"
	 * attribute.
	 */
	FILTER_HTTP_URI = 24,
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
 * The header of a message.  It is followed by #size bytes of payload.
 * 
 * Everything is network byte order (big-endian).
 */
struct PondHeader {
	/**
	 * A transaction identifier: all messages that belong to one
	 * transaction (e.g. #QUERY, #FILTER_SITE, #COMMIT) must have
	 * the same value.  The client generates this identifier at
	 * the start of each transaction, e.g. by starting with 1 and
	 * incrementing by one for each new transaction.  Replies from
	 * the server will have the same identifier.  The identifiers
	 * are local to this connection.
	 */
	uint16_t id;

	/**
	 * Either #PondRequestCommand (client-to-server) or
	 * #PondResponseCommand (server-to-client).
	 */
	uint16_t command;

	/**
	 * The size of the payload following this header.
	 */
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

/**
 * Payload for PondRequestCommand::WINDOW.
 */
struct PondWindowPayload {
	/**
	 * How many records will be sent at most with this query?
	 */
	uint64_t max;

	/**
	 * How many records will be skipped with this query?
	 */
	uint64_t skip;
};

/**
 * Payload for PondRequestCommand::FILTER_HTTP_STATUS.
 */
struct PondFilterHttpStatusPayload {
	uint16_t begin, end;
};
