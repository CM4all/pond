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

#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

static constexpr Event::Duration max_age_interval = std::chrono::minutes(1);

Instance::Instance(const Config &config)
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 avahi_client(event_loop, "Pond"),
	 max_age(config.database.max_age),
	 max_age_timer(event_loop, BIND_THIS_METHOD(OnMaxAgeTimer)),
	 database(config.database.size)
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
	auto &listener = listeners.front();

	if (!config.zeroconf_service.empty()) {
		/* ask the kernel for the effective address via
		   getsockname(), because it may have changed, e.g. if
		   the kernel has selected a port for us */
		const auto local_address = listener.GetLocalAddress();
		if (local_address.IsDefined()) {
			const char *const interface = config.interface.empty()
				? nullptr
				: config.interface.c_str();

			avahi_client.AddService(config.zeroconf_service.c_str(),
						interface, local_address);
		}
	}
}

void
Instance::AddConnection(UniqueSocketDescriptor &&fd) noexcept
{
	auto *c = new Connection(*this, std::move(fd));
	connections.push_front(*c);
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

	should_exit = true;

	shutdown_listener.Disable();
	sighup_event.Disable();

	avahi_client.Close();

	receivers.clear();

	connections.clear_and_dispose(DeleteDisposer());

	listeners.clear();
}

void
Instance::OnReload(int) noexcept
{
}
