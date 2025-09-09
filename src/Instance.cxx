// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Config.hxx"
#include "Listener.hxx"
#include "Connection.hxx"
#include "Protocol.hxx"
#include "event/net/MultiUdpListener.hxx"
#include "net/SocketConfig.hxx"
#include "net/StaticSocketAddress.hxx"
#include "util/ByteOrder.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/PrintException.hxx"
#include "config.h"

#ifdef HAVE_AVAHI
#include "lib/avahi/Client.hxx"
#include "lib/avahi/Publisher.hxx"
#include "lib/avahi/Service.hxx"
#endif

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <cassert>

#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

static constexpr Event::Duration max_age_interval = std::chrono::minutes(1);

Instance::Instance(const Config &config)
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 max_age(config.database.max_age),
	 max_age_timer(event_loop, BIND_THIS_METHOD(OnMaxAgeTimer)),
	 database(config.database.size,
		  config.database.per_site_message_rate_limit)
{
	shutdown_listener.Enable();
	sighup_event.Enable();
}

Instance::~Instance() noexcept = default;

PondStatsPayload
Instance::GetStats() const noexcept
{
	PondStatsPayload s{};
	s.memory_capacity = ToBE64(database.GetMemoryCapacity());
	s.memory_usage = ToBE64(database.GetMemoryUsage());
	s.n_records = ToBE64(database.GetRecordCount());
	s.n_received = ToBE64(n_received);
	s.n_malformed = ToBE64(n_malformed);
	s.n_discarded = ToBE64(n_discarded);
	return s;
}

#ifdef HAVE_AVAHI

Avahi::Client &
Instance::GetAvahiClient()
{
	if (!avahi_client) {
		Avahi::ErrorHandler &error_handler = *this;
		avahi_client = std::make_unique<Avahi::Client>(event_loop,
							       error_handler);
	}

	return *avahi_client;
}

void
Instance::EnableZeroconf() noexcept
{
	assert(!avahi_publisher);

	if (avahi_services.empty())
		return;

	/* note: avahi_services is passed as a copy because we may
	   need to disable and re-enable Zeroconf for a manual CLONE
	   operation */

	Avahi::ErrorHandler &error_handler = *this;
	avahi_publisher = std::make_unique<Avahi::Publisher>(GetAvahiClient(),
							     "Pond",
							     error_handler);

	for (auto &i : avahi_services)
		avahi_publisher->AddService(i);
}

void
Instance::DisableZeroconf() noexcept
{
	if (!avahi_publisher)
		return;

	for (auto &i : avahi_services)
		avahi_publisher->RemoveService(i);

	avahi_publisher.reset();
}

#endif // HAVE_AVAHI

void
Instance::AddReceiver(const SocketConfig &config)
{
	UdpHandler &handler = *this;
	receivers.emplace_front(event_loop,
				config.Create(SOCK_DGRAM),
				MultiReceiveMessage(256, MAX_DATAGRAM_SIZE),
				handler);

	static constexpr int buffer_size = 4 * 1024 * 1024;
	receivers.front().GetSocket().SetOption(SOL_SOCKET, SO_RCVBUF,
						&buffer_size, sizeof(buffer_size));
	receivers.front().GetSocket().SetOption(SOL_SOCKET, SO_RCVBUFFORCE,
						&buffer_size, sizeof(buffer_size));
}

void
Instance::AddListener(const ListenerConfig &config)
{
	listeners.emplace_front(*this,
				config.Create(SOCK_STREAM));
#ifdef HAVE_AVAHI
	auto &listener = listeners.front();

	if (config.zeroconf.IsEnabled()) {
		/* ask the kernel for the effective address via
		   getsockname(), because it may have changed, e.g. if
		   the kernel has selected a port for us */
		if (const auto local_address = listener.GetSocket().GetLocalAddress();
		    local_address.IsDefined()) {
			const char *const interface = config.interface.empty()
				? nullptr
				: config.interface.c_str();

			avahi_services.emplace_front(config.zeroconf,
						     interface, local_address,
						     config.v6only);
		}
	}
#endif // HAVE_AVAHI
}

void
Instance::AddConnection(UniqueSocketDescriptor &&fd) noexcept
{
	auto *c = new Connection(*this, std::move(fd));
	connections.push_front(*c);
}

void
Instance::SetBlockingOperation(std::unique_ptr<BlockingOperation> op) noexcept
{
	assert(!blocking_operation);

#ifdef HAVE_AVAHI
	DisableZeroconf();
#endif

	blocking_operation = std::move(op);
}

void
Instance::OnOperationFinished() noexcept
{
	assert(blocking_operation);
#ifdef HAVE_AVAHI
	assert(!avahi_publisher);
#endif // HAVE_AVAHI

	blocking_operation.reset();

#ifdef HAVE_AVAHI
	EnableZeroconf();
#endif
}

void
Instance::OnMaxAgeTimer() noexcept
{
	assert(max_age > std::chrono::system_clock::duration::zero());

	database.DeleteOlderThan(Net::Log::FromSystem(event_loop.SystemNow() - max_age));
}

void
Instance::MaybeScheduleMaxAgeTimer() noexcept
{
	if (max_age > std::chrono::system_clock::duration::zero() &&
	    !max_age_timer.IsPending())
		max_age_timer.Schedule(max_age_interval);
}

void
Instance::OnExit() noexcept
{
	if (should_exit)
		return;

#ifdef HAVE_LIBSYSTEMD
	sd_notify(0, "STOPPING=1");
#endif

	should_exit = true;

	shutdown_listener.Disable();
	sighup_event.Disable();

	blocking_operation.reset();

	max_age_timer.Cancel();

#ifdef HAVE_AVAHI
	DisableZeroconf();
	avahi_client.reset();
#endif // HAVE_AVAHI

	receivers.clear();

	connections.clear_and_dispose(DeleteDisposer());

	listeners.clear();
}

void
Instance::OnReload(int) noexcept
{
}

#ifdef HAVE_AVAHI

bool
Instance::OnAvahiError(std::exception_ptr e) noexcept
{
	PrintException(e);
	return true;
}

#endif // HAVE_AVAHI
