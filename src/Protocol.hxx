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
};

enum class PondResponseCommand : uint16_t {
	NOP,

	/**
	 * End of the current response.  Needed for some types of
	 * responses.
	 */
	END,
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
