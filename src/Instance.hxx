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

#pragma once

#include "BlockingOperation.hxx"
#include "Database.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/net/UdpHandler.hxx"
#include "io/Logger.hxx"
#include "util/IntrusiveList.hxx"

#include <forward_list>
#include <memory>

#include <stdint.h>

struct Config;
struct SocketConfig;
struct ListenerConfig;
struct PondStatsPayload;
class UniqueSocketDescriptor;
class MultiUdpListener;
class Listener;
class Connection;
namespace Avahi { class Client; class Publisher; struct Service; }

class Instance final : FullUdpHandler, public BlockingOperationHandler {
	static constexpr size_t MAX_DATAGRAM_SIZE = 4096;

	const RootLogger logger;

	EventLoop event_loop;

	bool should_exit = false;

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;

	std::unique_ptr<Avahi::Client> avahi_client;
	std::forward_list<Avahi::Service> avahi_services;
	std::unique_ptr<Avahi::Publisher> avahi_publisher;

	std::forward_list<MultiUdpListener> receivers;
	std::forward_list<Listener> listeners;

	IntrusiveList<Connection> connections;

	/**
	 * An operation which blocks this daemon; Zeroconf
	 * announcements and all receivers will be disabled while it
	 * runs.  For example, this could be a CLONE.
	 */
	std::unique_ptr<BlockingOperation> blocking_operation;

	const std::chrono::system_clock::duration max_age{};

	/**
	 * This timer deletes old records once a minute if a #max_age
	 * was configured.
	 */
	CoarseTimerEvent max_age_timer;

	Database database;

	/**
	 * @see struct PondStatsPayload
	 */
	uint64_t n_received = 0, n_malformed = 0, n_discarded = 0;

public:
	explicit Instance(const Config &config);
	~Instance() noexcept;

	const RootLogger &GetLogger() const noexcept {
		return logger;
	}

	EventLoop &GetEventLoop() noexcept {
		return event_loop;
	}

	Database &GetDatabase() noexcept {
		return database;
	}

	const Database &GetDatabase() const noexcept {
		return database;
	}

	gcc_pure
	PondStatsPayload GetStats() const noexcept;

	Avahi::Client &GetAvahiClient();

	void EnableZeroconf() noexcept;

	void AddReceiver(const SocketConfig &config);
	void AddListener(const ListenerConfig &config);
	void AddConnection(UniqueSocketDescriptor &&fd) noexcept;

	bool IsBlocked() const noexcept {
		return !!blocking_operation;
	}

	void SetBlockingOperation(std::unique_ptr<BlockingOperation> op) noexcept;

	void CancelBlockingOperation() noexcept {
		blocking_operation.reset();
	}

	void Dispatch() noexcept {
		event_loop.Dispatch();
	}

private:
	void OnMaxAgeTimer() noexcept;

	/**
	 * Schedule the #max_age_timer if a #max_age is configured
	 * (but don't update the timer if it has already been
	 * scheduled).
	 */
	void MaybeScheduleMaxAgeTimer() noexcept;

	void OnExit() noexcept;
	void OnReload(int) noexcept;

	/* virtual methods from UdpHandler */
	bool OnUdpDatagram(ConstBuffer<void> payload,
			   WritableBuffer<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr ep) noexcept override;

	/* virtual methods from BlockingOperationHandler */
	void OnOperationFinished() noexcept override;
};
