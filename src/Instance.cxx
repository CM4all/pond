/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "net/UdpListener.hxx"
#include "net/SocketConfig.hxx"

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
Instance::OnExit()
{
	if (should_exit)
		return;

	should_exit = true;

	shutdown_listener.Disable();
	sighup_event.Disable();

	receivers.clear();
}

void
Instance::OnReload(int)
{
}
