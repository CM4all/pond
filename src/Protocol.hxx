/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <stdint.h>

enum class PondRequestCommand : uint16_t {
	NOP,

	/**
	 * Commit the current request parameters and start executing
	 * the request.
	 */
	COMMIT,

	/**
	 * Cancel this request.
	 */
	CANCEL,

	/**
	 * Query records.  This packet initiates a new request.
	 */
	QUERY,

	/**
	 * Specify a filter on the "site" attribute.  Payload is the
	 * exact string to compare with.
	 */
	FILTER_SITE,

	/**
	 * Option for #QUERY which follows incoming new records
	 * instead of printing past ones.  The response never ends
	 * until the client sends #CANCEL (or closes the connection).
	 */
	FOLLOW,

	/**
	 * Specify a filter on the "timestamp" attribute.  Payload is
	 * a 64 bit time stamp (microseconds since epoch).
	 */
	FILTER_SINCE,

	/**
	 * Specify a filter on the "timestamp" attribute.  Payload is
	 * a 64 bit time stamp (microseconds since epoch).
	 */
	FILTER_UNTIL,

	/**
	 * Option for #QUERY which limits the number of distinct sites
	 * in the response.  This is useful to write site-specific log
	 * files while keeping only a certain number of files open.
	 * Payload is #PondGroupSitePayload.
	 */
	GROUP_SITE,
};

enum class PondResponseCommand : uint16_t {
	NOP,

	/**
	 * An error has occurred.  Payload is a human-readable error
	 * message.
	 */
	ERROR,

	/**
	 * End of the current response.  Needed for some types of
	 * responses.
	 */
	END,

	/**
	 * A serialized log record according to net/log/Protocol.hxx
	 */
	LOG_RECORD,
};

/**
 * Everything is network byte order (big-endian).
 */
struct PondHeader {
	uint16_t id;
	uint16_t command;
	uint16_t size;
};

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

static_assert(sizeof(PondHeader) == 6, "Wrong size");
