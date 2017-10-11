/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Database.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "net/UdpHandler.hxx"
#include "io/Logger.hxx"

#include <forward_list>

struct UdpListenerConfig;
class UdpListener;

class Instance final : UdpHandler {
	RootLogger logger;

	EventLoop event_loop;

	bool should_exit = false;

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;

	std::forward_list<UdpListener> receivers;

	Database database;

public:
	Instance();
	~Instance();

	EventLoop &GetEventLoop() {
		return event_loop;
	}

	void AddReceiver(const UdpListenerConfig &config);

	void Dispatch() {
		event_loop.Dispatch();
	}

private:
	void OnExit();
	void OnReload(int);

	void OnSystemdAgentReleased(const char *path);

	/* virtual methods from UdpHandler */
	void OnUdpDatagram(const void *data, size_t length,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr ep) override;
};
