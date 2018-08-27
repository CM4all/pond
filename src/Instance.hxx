/*
 * Copyright 2017-2018 Content Management AG
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

#include "Database.hxx"
#include "avahi/Client.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "event/net/UdpHandler.hxx"
#include "io/Logger.hxx"

#include <boost/intrusive/list.hpp>

#include <forward_list>

struct Config;
struct SocketConfig;
struct ListenerConfig;
class UniqueSocketDescriptor;
class MultiUdpListener;
class Listener;
class Connection;

class Instance final : UdpHandler {
	const RootLogger logger;

	EventLoop event_loop;

	bool should_exit = false;

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;

	MyAvahiClient avahi_client;

	std::forward_list<MultiUdpListener> receivers;
	std::forward_list<Listener> listeners;

	boost::intrusive::list<Connection,
			       boost::intrusive::base_hook<boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>>,
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
	void AddListener(const ListenerConfig &config);
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
