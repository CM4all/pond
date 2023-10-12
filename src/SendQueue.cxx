// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

		ssize_t nbytes = s.Send(buffer, MSG_DONTWAIT);
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
