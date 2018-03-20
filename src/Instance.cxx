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
#include "event/net/MultiUdpListener.hxx"
#include "net/SocketConfig.hxx"
#include "util/DeleteDisposer.hxx"

#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

Instance::Instance(const Config &config)
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 database(config.database.size)
{
	shutdown_listener.Enable();
	sighup_event.Enable();
}

Instance::~Instance()
{
}

void
Instance::AddReceiver(const SocketConfig &config)
{
	UdpHandler &handler = *this;
	receivers.emplace_front(event_loop,
				config.Create(SOCK_DGRAM),
				MultiReceiveMessage(256, 4096),
				handler);

	static constexpr int buffer_size = 4 * 1024 * 1024;
	receivers.front().GetSocket().SetOption(SOL_SOCKET, SO_RCVBUF,
						&buffer_size, sizeof(buffer_size));
	receivers.front().GetSocket().SetOption(SOL_SOCKET, SO_RCVBUFFORCE,
						&buffer_size, sizeof(buffer_size));
}

void
Instance::AddListener(const SocketConfig &config)
{
	listeners.emplace_front(*this,
				config.Create(SOCK_STREAM));
}

void
Instance::AddConnection(UniqueSocketDescriptor &&fd)
{
	auto *c = new Connection(*this, std::move(fd));
	connections.push_front(*c);
}

void
Instance::OnExit()
{
	if (should_exit)
		return;

	should_exit = true;

	shutdown_listener.Disable();
	sighup_event.Disable();

	receivers.clear();

	connections.clear_and_dispose(DeleteDisposer());

	listeners.clear();
}

void
Instance::OnReload(int)
{
}
