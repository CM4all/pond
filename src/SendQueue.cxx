/*
 * Copyright 2017-2022 CM4all GmbH
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

#include "SendQueue.hxx"
#include "net/SocketDescriptor.hxx"
#include "system/Error.hxx"

#include <cassert>
#include <cerrno>

#include <sys/socket.h>

void
SendQueue::Push(const struct iovec &v, std::size_t skip) noexcept
{
	std::span<const std::byte> src{(const std::byte *)v.iov_base, v.iov_len};
	src = src.subspan(skip);
	if (!src.empty())
		queue.emplace(src);
}

bool
SendQueue::Flush(SocketDescriptor s)
{
	// TODO: use sendmsg() to combine all send() calls into one

	while (!queue.empty()) {
		std::span<const std::byte> buffer = queue.front();
		assert(buffer.size() > consumed);
		buffer = buffer.subspan(consumed);
		assert(!buffer.empty());

		ssize_t nbytes = send(s.Get(), buffer.data(), buffer.size(),
				      MSG_DONTWAIT|MSG_NOSIGNAL);
		if (nbytes < 0) {
			const int e = errno;
			if (e == EAGAIN)
				return false;

			throw MakeErrno(e, "Failed to send");
		}

		if (std::size_t(nbytes) < buffer.size()) {
			consumed += nbytes;
			return false;
		}

		queue.pop();
		consumed = 0;
	}

	return true;
}
