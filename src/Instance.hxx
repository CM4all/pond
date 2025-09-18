// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "BlockingOperation.hxx"
#include "Database.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/FarTimerEvent.hxx"
#include "event/net/UdpHandler.hxx"
#include "io/Logger.hxx"
#include "util/IntrusiveList.hxx"
#include "config.h"

#ifdef HAVE_AVAHI
#include "lib/avahi/ErrorHandler.hxx"
#endif

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

class Instance final
	: UdpHandler,
#ifdef HAVE_AVAHI
	  Avahi::ErrorHandler,
#endif
	  public BlockingOperationHandler
{
	static constexpr size_t MAX_DATAGRAM_SIZE = 4096;

	const RootLogger logger;

	EventLoop event_loop;

	bool should_exit = false;

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;

#ifdef HAVE_AVAHI
	std::unique_ptr<Avahi::Client> avahi_client;
	std::forward_list<Avahi::Service> avahi_services;
	std::unique_ptr<Avahi::Publisher> avahi_publisher;
#endif // HAVE_AVAHI

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

	/**
	 * Call Database::Compress() once an hour.
	 */
	FarTimerEvent compress_timer{event_loop, BIND_THIS_METHOD(OnCompressTimer)};

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

	[[gnu::pure]]
	PondStatsPayload GetStats() const noexcept;

#ifdef HAVE_AVAHI
	Avahi::Client &GetAvahiClient();

	void EnableZeroconf() noexcept;
	void DisableZeroconf() noexcept;
#endif // HAVE_AVAHI

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

	void Run() noexcept {
		event_loop.Run();
	}

private:
	void OnMaxAgeTimer() noexcept;
	void OnCompressTimer() noexcept;

	/**
	 * Schedule the #max_age_timer if a #max_age is configured
	 * (but don't update the timer if it has already been
	 * scheduled).
	 */
	void MaybeScheduleMaxAgeTimer() noexcept;

	void OnExit() noexcept;
	void OnReload(int) noexcept;

	/* virtual methods from UdpHandler */
	bool OnUdpDatagram(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr &&error) noexcept override;

	/* virtual methods from BlockingOperationHandler */
	void OnOperationFinished() noexcept override;

#ifdef HAVE_AVAHI
	/* virtual methods from class Avahi::ErrorHandler */
	bool OnAvahiError(std::exception_ptr e) noexcept override;
#endif // HAVE_AVAHI
};
