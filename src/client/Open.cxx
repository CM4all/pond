/*
 * Copyright 2017-2020 CM4all GmbH
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

#include "Open.hxx"
#include "Port.hxx"
#include "avahi/Client.hxx"
#include "avahi/ErrorHandler.hxx"
#include "avahi/Explorer.hxx"
#include "avahi/ExplorerListener.hxx"
#include "event/Loop.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/net/ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/RConnectSocket.hxx"

#include <list>
#include <stdexcept>

#include <assert.h>

static constexpr Event::Duration CONNECT_TIMEOUT = std::chrono::seconds(30);

class ConnectZeroconfOperation final
	: Avahi::ServiceExplorerListener, Avahi::ErrorHandler,
	  ConnectSocketHandler {

	Avahi::Client client;

	Avahi::ServiceExplorer explorer;
	CoarseTimerEvent explorer_timeout;

	struct Server {
		std::string key;
		AllocatedSocketAddress address;

		template<typename K, typename A>
		Server(K &&_key, A &&_address) noexcept
			:key(std::forward<K>(_key)),
			 address(std::forward<A>(_address)) {}
	};

	std::list<Server> servers;

	ConnectSocket connect;

	UniqueSocketDescriptor result;
	std::exception_ptr error;

public:
	ConnectZeroconfOperation(EventLoop &event_loop,
				 const char *service_name) noexcept
		:client(event_loop, *this),
		 explorer(client, *this,
			  AVAHI_IF_UNSPEC,
			  AVAHI_PROTO_UNSPEC,
			  service_name,
			  nullptr,
			  *this),
		 explorer_timeout(client.GetEventLoop(),
				  BIND_THIS_METHOD(OnExplorerTimeout)),
		 connect(client.GetEventLoop(), *this) {
		explorer_timeout.Schedule(std::chrono::seconds(5));
	}

	auto &GetEventLoop() const noexcept {
		return explorer_timeout.GetEventLoop();
	}

	UniqueSocketDescriptor GetResult() {
		if (result.IsDefined())
			return std::move(result);

		std::rethrow_exception(error);
	}

private:
	void OnExplorerTimeout() noexcept {
		if (servers.empty()) {
			if (!error)
				error = std::make_exception_ptr(std::runtime_error("No server found"));
			GetEventLoop().Break();
		}
	}

	/* virtual methods from class AvahiServiceExplorerListener */
	void OnAvahiNewObject(const std::string &key,
			      SocketAddress address) noexcept override {
		const bool was_empty = servers.empty();
		servers.emplace_back(key, address);

		if (was_empty)
			connect.Connect(servers.front().address,
					CONNECT_TIMEOUT);
	}

	void OnAvahiRemoveObject(const std::string &key) noexcept override {
		for (auto i = servers.begin(), end = servers.end(); i != end;) {
			if (i->key == key)
				i = servers.erase(i);
			else
				++i;
		}
	}

	/* virtual methods from class Avahi::ErrorHandler */
	bool OnAvahiError(std::exception_ptr e) noexcept override {
		if (!error)
			error = std::move(e);
		GetEventLoop().Break();
		return false;
	}

	/* virtual methods from class ConnectSocketHandler */
	void OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept override {
		result = std::move(fd);
		GetEventLoop().Break();
	}

	void OnSocketConnectError(std::exception_ptr ep) noexcept override {
		if (!error)
			error = ep;

		if (!servers.empty())
			connect.Connect(servers.front().address,
					CONNECT_TIMEOUT);
		else if (!explorer_timeout.IsPending())
			GetEventLoop().Break();
	}
};

static UniqueSocketDescriptor
ConnectZeroconf(const char *service_name)
{
	EventLoop event_loop;
	ConnectZeroconfOperation operation(event_loop, service_name);
	event_loop.Dispatch();
	return operation.GetResult();
}

UniqueSocketDescriptor
PondConnect(const PondServerSpecification &spec)
{
	if (!spec.zeroconf_service.empty())
		return ConnectZeroconf(spec.zeroconf_service.c_str());

	assert(spec.host != nullptr);
	return ResolveConnectStreamSocket(spec.host, POND_DEFAULT_PORT);
}
