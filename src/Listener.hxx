/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "event/net/ServerSocket.hxx"

class Instance;
class RootLogger;

class Listener final : ServerSocket {
	Instance &instance;
	const RootLogger &logger;

public:
	Listener(Instance &_instance, UniqueSocketDescriptor &&fd);

private:
	/* virtual methods from class ServerSocket */
	void OnAccept(UniqueSocketDescriptor &&fd,
		      SocketAddress address) override;
	void OnAcceptError(std::exception_ptr ep) override;
};
