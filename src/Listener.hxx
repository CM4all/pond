// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/net/ServerSocket.hxx"

class Instance;
class RootLogger;

class Listener final : ServerSocket {
	Instance &instance;
	const RootLogger &logger;

public:
	Listener(Instance &_instance, UniqueSocketDescriptor &&fd);

	using ServerSocket::GetLocalAddress;

private:
	/* virtual methods from class ServerSocket */
	void OnAccept(UniqueSocketDescriptor fd,
		      SocketAddress address) noexcept override;
	void OnAcceptError(std::exception_ptr ep) noexcept override;
};
