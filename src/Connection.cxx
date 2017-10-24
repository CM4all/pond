/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Connection.hxx"
#include "Protocol.hxx"
#include "Instance.hxx"
#include "event/Duration.hxx"
#include "util/ByteOrder.hxx"

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

inline BufferedResult
Connection::OnPacket(uint16_t id, PondRequestCommand cmd,
		     ConstBuffer<void> payload)
{
	fprintf(stderr, "id=0x%04x cmd=%d payload=%zu\n", id, unsigned(cmd), payload.size);

	return BufferedResult::OK;
}

BufferedResult
Connection::OnBufferedData(const void *buffer, size_t size)
{
	if (size < sizeof(PondHeader))
		return BufferedResult::MORE;

	const auto *be_header = (const PondHeader *)buffer;

	const size_t payload_size = FromBE16(be_header->size);
	if (size < sizeof(PondHeader) + payload_size)
		return BufferedResult::MORE;

	const uint16_t id = FromBE16(be_header->id);
	const auto command = PondRequestCommand(FromBE16(be_header->command));

	size_t consumed = sizeof(*be_header) + payload_size;
	socket.Consumed(consumed);

	auto result = OnPacket(id, command, {be_header + 1, payload_size});
	if (result == BufferedResult::OK && consumed < size)
		result = BufferedResult::PARTIAL;

	return BufferedResult::OK;
}

bool
Connection::OnBufferedClosed() noexcept
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
Connection::OnBufferedError(std::exception_ptr e) noexcept
{
	logger(2, e);
	Destroy();
}
