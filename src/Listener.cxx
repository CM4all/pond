// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Listener.hxx"
#include "Instance.hxx"
#include "net/SocketAddress.hxx"

Listener::Listener(Instance &_instance, UniqueSocketDescriptor &&_fd)
	:ServerSocket(_instance.GetEventLoop(), std::move(_fd)),
	 instance(_instance), logger(instance.GetLogger()) {}

void
Listener::OnAccept(UniqueSocketDescriptor connection_fd,
		   SocketAddress) noexcept
{
	instance.AddConnection(std::move(connection_fd));
}

void
Listener::OnAcceptError(std::exception_ptr ep) noexcept
{
	logger(1, "TCP accept error: ", ep);
}
