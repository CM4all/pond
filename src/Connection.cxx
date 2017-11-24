/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Connection.hxx"
#include "Instance.hxx"
#include "FCursor.hxx"
#include "event/Duration.hxx"
#include "system/Error.hxx"
#include "util/ByteOrder.hxx"
#include "util/StringView.hxx"

#include <array>

void
Connection::Request::Clear()
{
	command = PondRequestCommand::NOP;
	filter = Filter();
	follow = false;
	cursor.reset();
}

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

static PondHeader
MakeHeader(uint16_t id, PondResponseCommand command, size_t size)
{
	PondHeader header;
	if (size >= std::numeric_limits<decltype(header.size)>::max())
		throw std::runtime_error("Payload is too large");

	header.id = ToBE16(id);
	header.command = ToBE16(uint16_t(command));
	header.size = ToBE16(size);

	return header;
}

struct PondIovec {
	PondHeader header;
	std::array<struct iovec, 2> vec;

	constexpr size_t GetTotalSize() const {
		return vec[0].iov_len + vec[1].iov_len;
	}
};

static unsigned
MakeIovec(PondIovec &pi,
	  uint16_t id, PondResponseCommand command,
	  ConstBuffer<void> payload)
{
	pi.header = MakeHeader(id, command, payload.size);
	pi.vec[0].iov_base = const_cast<void *>((const void *)&pi.header);
	pi.vec[0].iov_len = sizeof(pi.header);
	pi.vec[1].iov_base = const_cast<void *>(payload.data);
	pi.vec[1].iov_len = payload.size;
	return 1u + !payload.empty();
}

void
Connection::Send(uint16_t id, PondResponseCommand command,
		 ConstBuffer<void> payload)
{
	PondIovec pi;
	const auto n = MakeIovec(pi, id, command, payload);

	ssize_t nbytes = socket.WriteV(&pi.vec.front(), n);
	if (nbytes < 0)
		throw MakeErrno("Failed to send");

	if (size_t(nbytes) != pi.GetTotalSize())
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

inline BufferedResult
Connection::OnPacket(uint16_t id, PondRequestCommand cmd,
		     ConstBuffer<void> payload)
try {
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
			current.cursor.reset(new FilteredCursor(instance.GetDatabase(),
								current.filter,
								BIND_THIS_METHOD(OnAppend)));
			if (current.follow) {
				current.cursor->Follow();
			} else {
				current.cursor->Rewind();
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

		if (!current.filter.site.empty())
			throw SimplePondError{"Duplicate FILTER_SITE"};

		if (!IsNonEmptyString(payload))
			throw SimplePondError{"Malformed FILTER_SITE"};

		current.filter.site.assign((const char *)payload.data,
					   payload.size);
		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::FILTER_SINCE:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_SINCE"};

		if (current.filter.since != 0)
			throw SimplePondError{"Duplicate FILTER_SINCE"};

		if (payload.size != sizeof(uint64_t))
			throw SimplePondError{"Malformed FILTER_SITE"};

		current.filter.since = FromBE64(*(const uint64_t *)payload.data);
		if (current.filter.since == 0)
			throw SimplePondError{"Malformed FILTER_SINCE"};
		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::FILTER_UNTIL:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_UNTIL"};

		if (current.filter.until != 0)
			throw SimplePondError{"Duplicate FILTER_UNTIL"};

		if (payload.size != sizeof(uint64_t))
			throw SimplePondError{"Malformed FILTER_SITE"};

		current.filter.until = FromBE64(*(const uint64_t *)payload.data);
		if (current.filter.until == 0)
			throw SimplePondError{"Malformed FILTER_UNTIL"};
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
Connection::OnAppend() noexcept
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

static unsigned
SendMulti(SocketDescriptor s, uint16_t id,
	  FilteredCursor cursor)
{
	constexpr unsigned CAPACITY = 16;

	std::array<PondIovec, CAPACITY> vecs;
	std::array<struct mmsghdr, CAPACITY> msgs;

	unsigned n = 0;
	do {
		const auto &record = *cursor;
		auto &m = msgs[n].msg_hdr;
		auto &v = vecs[n];

		m.msg_name = nullptr;
		m.msg_namelen = 0;
		m.msg_iovlen = MakeIovec(v, id,
					 PondResponseCommand::LOG_RECORD,
					 record.GetRaw());
		m.msg_iov = &v.vec.front();
		m.msg_control = nullptr;
		m.msg_controllen = 0;
		m.msg_flags = 0;

		++n;
		++cursor;
	} while (cursor && n < CAPACITY);

	int result = sendmmsg(s.Get(), &msgs.front(), n,
			      MSG_DONTWAIT|MSG_NOSIGNAL);
	if (result < 0) {
		if (errno == EAGAIN)
			return 0;
		throw MakeErrno("Failed to send");
	}

	// TODO: check for short send in each msg?
	return result;
}

bool
Connection::OnBufferedWrite()
{
	assert(current.command == PondRequestCommand::QUERY);
	assert(current.cursor);

	auto &cursor = *current.cursor;
	cursor.FixDeleted();

	if (cursor) {
		cursor += SendMulti(socket.GetSocket(), current.id, cursor);
		if (cursor)
			return true;
	}

	if (current.follow) {
		cursor.Follow();
	} else {
		Send(current.id, PondResponseCommand::END, nullptr);
		current.Clear();
	}

	socket.UnscheduleWrite();
	return true;
}

void
Connection::OnBufferedError(std::exception_ptr e) noexcept
{
	logger(2, e);
	Destroy();
}
