// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "BlockingOperation.hxx"
#include "lib/avahi/ErrorHandler.hxx"
#include "lib/avahi/Explorer.hxx"
#include "lib/avahi/ExplorerListener.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "io/Logger.hxx"
#include "util/IntrusiveList.hxx"

struct ListenerConfig;
class EventLoop;
class Database;
namespace Avahi { class Client; }

class AutoCloneOperation final : public BlockingOperation,
				 Avahi::ServiceExplorerListener,
				 Avahi::ErrorHandler {
	LLogger logger;

	BlockingOperationHandler &handler;

	Database &db;

	Avahi::ServiceExplorer explorer;

	CoarseTimerEvent timeout_event;

	class Server;

	using ServerList = IntrusiveList<Server>;

	ServerList servers;

public:
	AutoCloneOperation(BlockingOperationHandler &_handler,
			   Database &_db,
			   Avahi::Client &avahi_client,
			   const ListenerConfig &listener) noexcept;

	~AutoCloneOperation() noexcept override;

	auto &GetEventLoop() const noexcept {
		return timeout_event.GetEventLoop();
	}

private:
	void OnTimeout() noexcept;

	void OnServerStats(Server &server) noexcept;
	void OnServerFinished(Server &server) noexcept;
	void OnServerError(Server &server, std::exception_ptr e) noexcept;

	/* virtual methods from class AvahiServiceExplorerListener */
	void OnAvahiNewObject(const std::string &key,
			      SocketAddress address) noexcept override;

	void OnAvahiRemoveObject(const std::string &) noexcept override {}

	/* virtual methods from class Avahi::ErrorHandler */
	bool OnAvahiError(std::exception_ptr e) noexcept override;
};
