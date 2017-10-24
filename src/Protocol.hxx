/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <stdint.h>

enum class PondCommand : uint16_t {
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

/**
 * Everything is network byte order (big-endian).
 */
struct PondHeader {
	uint16_t id;
	uint16_t command;
	uint16_t size;
};

static_assert(sizeof(PondHeader) == 6, "Wrong size");
