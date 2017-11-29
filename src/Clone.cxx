/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Connection.hxx"
#include "Error.hxx"
#include "Instance.hxx"
#include "client/Client.hxx"
#include "client/Datagram.hxx"
#include "system/Error.hxx"
#include "util/Macros.hxx"

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
		}
	}
}

static void
ConnectReceiveAndEmplace(Database &db, const char *address,
			 SocketDescriptor peer_socket)
{
	PondClient client(address);
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

	ConnectReceiveAndEmplace(instance.GetDatabase(),
				 current.address.c_str(),
				 socket.GetSocket());

	Send(current.id, PondResponseCommand::END, nullptr);
	current.Clear();
} catch (const std::runtime_error &e) {
	logger(2, "CLONE error: ", std::current_exception());
	throw SimplePondError{"CLONE error"};
}
