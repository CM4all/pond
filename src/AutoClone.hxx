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

#include "BlockingOperation.hxx"
#include "avahi/ErrorHandler.hxx"
#include "avahi/Explorer.hxx"
#include "avahi/ExplorerListener.hxx"
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
