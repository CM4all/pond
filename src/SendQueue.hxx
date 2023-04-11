// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/AllocatedArray.hxx"

#include <list>
#include <queue>

class SocketDescriptor;

/**
 * This helper class manages a queue of buffers which need to be sent
 * to a #SocketDescriptor.  It is used to queue buffers which were
 * sent partially.  This class creates a copy of the buffers, so the
 * caller my release the original buffer.
 */
class SendQueue final {
	using Buffer = AllocatedArray<std::byte>;

	std::queue<Buffer, std::list<Buffer>> queue;

	/**
	 * How much of the front buffer has already been consumed?
	 */
	std::size_t consumed = 0;

public:
	bool empty() const noexcept {
		return queue.empty();
	}

	/**
	 * Push data at the end of the queue.
	 *
	 * @param skip this number of bytes at the beginning of the
	 * buffer
	 */
	void Push(const struct iovec &v, std::size_t skip=0) noexcept;

	/**
	 * Throws on error.
	 *
	 * @return true if the queue is empty
	 */
	bool Flush(SocketDescriptor s);
};
