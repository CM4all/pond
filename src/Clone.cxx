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

#include "Connection.hxx"
#include "Port.hxx"
#include "Error.hxx"
#include "Instance.hxx"
#include "client/Client.hxx"
#include "client/Open.hxx"
#include "client/Datagram.hxx"
#include "net/RConnectSocket.hxx"
#include "system/Error.hxx"
#include "util/Macros.hxx"
#include "util/ScopeExit.hxx"

#include <poll.h>

static void
ReceiveAndEmplace(Database &db, PondClient &client, uint16_t id,
		  SocketDescriptor peer_socket)
{
	bool pending_clear = true;

	struct pollfd pfds[] = {
		{
			/* waiting for messages from the Pond
			   server */
			.fd = client.GetSocket().Get(),
			.events = POLLIN,
			.revents = 0,
		},
		{
			.fd = peer_socket.Get(),
			.events = POLLIN,
			.revents = 0,
		},
	};

	while (true) {
		if (client.IsEmpty()) {
			if (poll(pfds, ARRAY_SIZE(pfds), -1) < 0)
				throw MakeErrno("poll() failed");

			if (pfds[1].revents)
				/* our client has sent another packet or has
				   closed the connection; cancel this
				   operation */
				break;
		}

		const auto d = client.Receive();
		if (d.id != id)
			continue;

		switch (d.command) {
		case PondResponseCommand::NOP:
			break;

		case PondResponseCommand::ERROR:
			throw std::runtime_error(d.payload.ToString());

		case PondResponseCommand::END:
			return;

		case PondResponseCommand::LOG_RECORD:
			if (pending_clear) {
				/* postpone the Clear() call until we
				   have received at least one
				   datagram */
				pending_clear = false;
				db.Clear();
			}

			db.Emplace({d.payload.data.get(), d.payload.size});
			break;

		case PondResponseCommand::STATS:
			throw std::runtime_error("Unexpected response packet");
		}
	}
}

static void
ConnectReceiveAndEmplace(Database &db, const char *address,
			 SocketDescriptor peer_socket)
{
	PondClient client(ResolveConnectStreamSocket(address,
						     POND_DEFAULT_PORT));
	const auto id = client.MakeId();
	client.Send(id, PondRequestCommand::QUERY);
	client.Send(id, PondRequestCommand::COMMIT);

	ReceiveAndEmplace(db, client, id, peer_socket);
}

void
Connection::CommitClone()
try {
	if (!IsLocalAdmin())
		throw SimplePondError{"Forbiddden"};

	/* cloning can take a long time, and during that time, this
	   server is unavailable; better unregister it so clients
	   don't try to use it */
	instance.DisableZeroconf();
	AtScopeExit(&instance=instance) { instance.EnableZeroconf(); };

	ConnectReceiveAndEmplace(instance.GetDatabase(),
				 current.address.c_str(),
				 socket.GetSocket());

	Send(current.id, PondResponseCommand::END, nullptr);
	current.Clear();
} catch (...) {
	logger(2, "CLONE error: ", std::current_exception());
	throw SimplePondError{"CLONE error"};
}
