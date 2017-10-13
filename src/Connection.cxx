/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Connection.hxx"
#include "Instance.hxx"
#include "event/Duration.hxx"

Connection::Connection(Instance &_instance, UniqueSocketDescriptor &&_fd)
	:instance(_instance), logger(instance.GetLogger()),
	 socket(_instance.GetEventLoop())
{
	socket.Init(_fd.Release(), FD_TCP,
		    nullptr,
		    &EventDuration<30>::value,
		    *this);
	socket.DeferRead(false);
}

Connection::~Connection()
{
	if (socket.IsConnected())
		socket.Close();

	socket.Destroy();
}

BufferedResult
Connection::OnBufferedData(const void *buffer, size_t size)
{
	fprintf(stderr, "data: '%.*s'\n", int(size), (const char *)buffer);
	socket.Consumed(size);
	return BufferedResult::OK;
}

bool
Connection::OnBufferedClosed()
{
	Destroy();
	return false;
}

bool
Connection::OnBufferedWrite()
{
	// TODO
	return true;
}

void
Connection::OnBufferedError(std::exception_ptr e)
{
	logger(2, e);
	Destroy();
}
