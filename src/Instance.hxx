/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Database.hxx"
#include "Connection.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "event/net/UdpHandler.hxx"
#include "io/Logger.hxx"

#include <boost/intrusive/list.hpp>

#include <forward_list>

struct Config;
struct SocketConfig;
class UniqueSocketDescriptor;
class MultiUdpListener;
class Listener;

class Instance final : UdpHandler {
	const RootLogger logger;

	EventLoop event_loop;

	bool should_exit = false;

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;

	std::forward_list<MultiUdpListener> receivers;
	std::forward_list<Listener> listeners;

	boost::intrusive::list<Connection,
			       boost::intrusive::constant_time_size<false>> connections;

	Database database;

public:
	explicit Instance(const Config &config);
	~Instance();

	const RootLogger &GetLogger() const {
		return logger;
	}

	EventLoop &GetEventLoop() {
		return event_loop;
	}

	Database &GetDatabase() {
		return database;
	}

	const Database &GetDatabase() const {
		return database;
	}

	void AddReceiver(const SocketConfig &config);
	void AddListener(const SocketConfig &config);
	void AddConnection(UniqueSocketDescriptor &&fd);

	void Dispatch() {
		event_loop.Dispatch();
	}

private:
	void OnExit();
	void OnReload(int);

	/* virtual methods from UdpHandler */
	bool OnUdpDatagram(const void *data, size_t length,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr ep) noexcept override;
};
