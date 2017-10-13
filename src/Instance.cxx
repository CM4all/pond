/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Listener.hxx"
#include "Connection.hxx"
#include "event/net/UdpListener.hxx"
#include "net/SocketConfig.hxx"
#include "util/DeleteDisposer.hxx"

#include <signal.h>
#include <unistd.h>

Instance::Instance()
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload))
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
				handler);
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
