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
