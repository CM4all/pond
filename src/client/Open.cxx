// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Open.hxx"
#include "Port.hxx"
#include "event/Loop.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/net/ConnectSocket.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/RConnectSocket.hxx"

#ifdef HAVE_AVAHI
#include "lib/avahi/Client.hxx"
#include "lib/avahi/ErrorHandler.hxx"
#include "lib/avahi/Explorer.hxx"
#include "lib/avahi/ExplorerListener.hxx"
#endif

#include <list>
#include <stdexcept>

#include <assert.h>

static constexpr Event::Duration CONNECT_TIMEOUT = std::chrono::seconds(30);

#ifdef HAVE_AVAHI

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
			      SocketAddress address,
			      [[maybe_unused]] AvahiStringList *txt) noexcept override {
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
	event_loop.Run();
	return operation.GetResult();
}

#endif // HAVE_AVAHI

UniqueSocketDescriptor
PondConnect(const PondServerSpecification &spec)
{
#ifdef HAVE_AVAHI
	if (!spec.zeroconf_service.empty())
		return ConnectZeroconf(spec.zeroconf_service.c_str());
#endif // HAVE_AVAHI

	assert(spec.host != nullptr);
	return ResolveConnectStreamSocket(spec.host, POND_DEFAULT_PORT);
}
