/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Connection.hxx"
#include "Instance.hxx"
#include "event/Duration.hxx"
#include "system/Error.hxx"
#include "util/ByteOrder.hxx"
#include "util/StringView.hxx"

Connection::Connection(Instance &_instance, UniqueSocketDescriptor &&_fd)
	:instance(_instance), logger(instance.GetLogger()),
	 socket(_instance.GetEventLoop()),
	 current(_instance.GetDatabase(), BIND_THIS_METHOD(OnAppend))
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

void
Connection::Send(uint16_t id, PondResponseCommand command,
		 ConstBuffer<void> payload)
{
	PondHeader header;
	if (payload.size >= std::numeric_limits<decltype(header.size)>::max())
		throw std::runtime_error("Payload is too large");

	header.id = ToBE16(id);
	header.command = ToBE16(uint16_t(command));
	header.size = ToBE16(payload.size);

	struct iovec vec[] = {
		{
			.iov_base = &header,
			.iov_len = sizeof(header),
		},
		{
			.iov_base = const_cast<void *>(payload.data),
			.iov_len = payload.size,
		},
	};

	ssize_t nbytes = socket.WriteV(vec, 1u + !payload.empty());
	if (nbytes < 0)
		throw MakeErrno("Failed to send");

	if (size_t(nbytes) != sizeof(header) + payload.size)
		throw std::runtime_error("Short send");
}

struct SimplePondError {
	StringView message;
};

gcc_pure
static bool
HasNullByte(ConstBuffer<char> b) noexcept
{
	return std::find(b.begin(), b.end(), '\0') != b.end();
}

gcc_pure
static bool
IsNonEmptyString(ConstBuffer<char> b) noexcept
{
	return !b.empty() && !HasNullByte(b);
}

gcc_pure
static bool
IsNonEmptyString(ConstBuffer<void> b) noexcept
{
	return IsNonEmptyString(ConstBuffer<char>::FromVoid(b));
}

gcc_pure
static bool
MatchFilter(const char *value, const std::string &filter) noexcept
{
	return filter.empty() || (value != nullptr && filter == value);
}

gcc_pure
static bool
MatchFilter(const Net::Log::Datagram &d,
	    const std::string &filter_site) noexcept
{
	return MatchFilter(d.site, filter_site);
}

inline BufferedResult
Connection::OnPacket(uint16_t id, PondRequestCommand cmd,
		     ConstBuffer<void> payload)
try {
	fprintf(stderr, "id=0x%04x cmd=%d payload=%zu\n", id, unsigned(cmd), payload.size);

	if (current.IgnoreId(id))
		return BufferedResult::AGAIN_OPTIONAL;

	switch (cmd) {
	case PondRequestCommand::NOP:
		break;

	case PondRequestCommand::QUERY:
		socket.UnscheduleWrite();
		current.Set(id, cmd);
		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::COMMIT:
		if (!current.MatchId(id))
			throw SimplePondError{"Misplaced COMMIT"};

		switch (current.command) {
		case PondRequestCommand::QUERY:
			if (current.follow) {
				current.cursor.Follow();
			} else {
				current.cursor.Rewind();
				socket.ScheduleWrite();
			}
			/* the response will be assembled by
			   OnBufferedWrite() */
			return BufferedResult::AGAIN_OPTIONAL;

		default:
			throw SimplePondError{"Misplaced COMMIT"};
		}

	case PondRequestCommand::CANCEL:
		current.Clear();
		socket.UnscheduleWrite();
		break;

	case PondRequestCommand::FILTER_SITE:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_SITE"};

		if (!current.filter_site.empty())
			throw SimplePondError{"Duplicate FILTER_SITE"};

		if (!IsNonEmptyString(payload))
			throw SimplePondError{"Malformed FILTER_SITE"};

		current.filter_site.assign((const char *)payload.data,
					   payload.size);
		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::FOLLOW:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FOLLOW"};

		if (current.follow)
			throw SimplePondError{"Duplicate FOLLOW"};

		if (!payload.empty())
			throw SimplePondError{"Malformed FOLLOW"};

		current.follow = true;
		return BufferedResult::AGAIN_EXPECT;
	}

	throw SimplePondError{"Command not implemented"};
} catch (SimplePondError e) {
	Send(id, PondResponseCommand::ERROR, e.message.ToVoid());
	current.Clear();
	socket.UnscheduleWrite();
	return BufferedResult::AGAIN_OPTIONAL;
}

void
Connection::OnAppend()
{
	socket.ScheduleWrite();
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

	return result;
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
	assert(current.command == PondRequestCommand::QUERY);

	while (current.cursor &&
	       !MatchFilter(current.cursor->GetParsed(), current.filter_site))
		++current.cursor;

	if (current.cursor) {
		Send(current.id, PondResponseCommand::LOG_RECORD,
		     current.cursor->GetRaw());
		++current.cursor;
	} else if (current.follow) {
		socket.UnscheduleWrite();
		current.cursor.Follow();
	} else {
		Send(current.id, PondResponseCommand::END, nullptr);
		current.Clear();
		socket.UnscheduleWrite();
	}

	return true;
}

void
Connection::OnBufferedError(std::exception_ptr e) noexcept
{
	logger(2, e);
	Destroy();
}
