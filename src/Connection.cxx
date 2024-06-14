// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Error.hxx"
#include "Instance.hxx"
#include "Selection.hxx"
#include "io/Iovec.hxx"
#include "net/SocketError.hxx"
#include "net/log/Parser.hxx"
#include "util/ByteOrder.hxx"
#include "util/SpanCast.hxx"

#include <array>

#include <sys/socket.h>

void
Connection::Request::Clear() noexcept
{
	command = PondRequestCommand::NOP;
	filter = Filter();
	group_site.max_sites = 0;
	window.max = 0;
	follow = false;
	continue_ = false;
	selection.reset();
	address.clear();
}

Connection::Connection(Instance &_instance, UniqueSocketDescriptor &&_fd) noexcept
	:instance(_instance), logger(instance.GetLogger()),
	 socket(_instance.GetEventLoop())
{
	socket.Init(_fd.Release(), FD_TCP,
		    std::chrono::seconds(30),
		    *this);
	socket.DeferRead();
}

Connection::~Connection() noexcept = default;

bool
Connection::IsLocalAdmin() const noexcept
{
	const auto cred = GetSocket().GetPeerCredentials();
	return cred.pid != -1 && (cred.uid == 0 || cred.uid == geteuid());
}

static constexpr PondHeader
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

	constexpr size_t GetTotalSize() const noexcept {
		return vec[0].iov_len + vec[1].iov_len;
	}

	void Queue(SendQueue &queue, std::size_t sent) noexcept {
		for (const auto &i : vec) {
			if (sent < i.iov_len) {
				queue.Push(i, sent);
				sent = 0;
			} else {
				sent -= i.iov_len;
			}
		}
	}
};

static unsigned
MakeIovec(PondIovec &pi,
	  uint16_t id, PondResponseCommand command,
	  std::span<const std::byte> payload) noexcept
{
	pi.header = MakeHeader(id, command, payload.size());
	pi.vec[0] = MakeIovecT(pi.header);
	pi.vec[1] = MakeIovec(payload);
	return 1u + !payload.empty();
}

void
Connection::Send(uint16_t id, PondResponseCommand command,
		 std::span<const std::byte> payload)
{
	PondIovec pi;
	const auto n = MakeIovec(pi, id, command, payload);

	if (!send_queue.empty()) {
		/* some data is still queued due to EAGAIN; instead of
		   sending directly (which will fail with EAGAIN),
		   push it to the end of the send queue */

		if (current.command != PondRequestCommand::QUERY ||
		    command != PondResponseCommand::END)
			/* allow queueing only if this is an attempt
			   to send an END packet to finish a QUERY
			   response */
			throw std::runtime_error("Pipelining not supported");

		for (const auto &i : pi.vec)
			send_queue.Push(i);
		return;
	}

	ssize_t nbytes = socket.WriteV(std::span{pi.vec}.first(n));
	if (nbytes < 0)
		throw MakeSocketError("Failed to send");

	if (size_t(nbytes) != pi.GetTotalSize())
		throw std::runtime_error("Short send");
}

static SiteIterator *
FindNonEmpty(Database &db, const Filter &filter, SiteIterator *i) noexcept
{
	while (i != nullptr) {
		auto selection = db.Select(*i, filter);
		if (selection)
			break;

		i = db.GetNextSite(*i);
	}

	return i;
}

static SiteIterator *
SkipNonEmpty(Database &db, const Filter &filter, SiteIterator *i,
	     unsigned n) noexcept
{
	if (i == nullptr)
		return nullptr;

	/* skip empty sites at the beginning */
	i = FindNonEmpty(db, filter, i);

	/* now skip "n" more sites (ignoring empty sites in
	   between) */
	while (i != nullptr && n-- > 0)
		i = FindNonEmpty(db, filter, db.GetNextSite(*i));

	return i;
}

inline void
Connection::CommitQuery() noexcept
{
	auto &db = instance.GetDatabase();

	current.site_iterator = nullptr;

	if (current.follow) {
		current.selection.reset(new Selection(db.Follow(current.filter, *this)));
	} else if (current.HasGroupSite()) {
		current.site_iterator = SkipNonEmpty(db, current.filter,
						     db.GetFirstSite(),
						     current.group_site.skip_sites);

		if (current.site_iterator == nullptr) {
			current.selection.reset(new Selection(AnyRecordList(), Filter()));
			socket.ScheduleWrite();
			return;
		}

		current.selection.reset(new Selection(db.Select(*current.site_iterator,
								current.filter)));
		socket.ScheduleWrite();
	} else {
		current.selection.reset(new Selection(db.Select(current.filter)));
		socket.ScheduleWrite();
	}

	/* the response will be assembled by
	   OnBufferedWrite() */
}

[[gnu::pure]]
static bool
HasNullByte(std::string_view b) noexcept
{
	return std::find(b.begin(), b.end(), '\0') != b.end();
}

[[gnu::pure]]
static bool
IsNonEmptyString(std::string_view b) noexcept
{
	return !b.empty() && !HasNullByte(b);
}

[[gnu::pure]]
static bool
IsNonEmptyString(std::span<const std::byte> b) noexcept
{
	return IsNonEmptyString(ToStringView(b));
}

inline BufferedResult
Connection::OnPacket(uint16_t id, PondRequestCommand cmd,
		     std::span<const std::byte> payload)
try {
	if (current.IgnoreId(id))
		return BufferedResult::AGAIN;

	switch (cmd) {
	case PondRequestCommand::NOP:
		break;

	case PondRequestCommand::QUERY:
		socket.UnscheduleWrite();
		AppendListener::Unregister();
		current.Set(id, cmd);
		return BufferedResult::AGAIN;

	case PondRequestCommand::COMMIT:
		if (!current.MatchId(id))
			throw SimplePondError{"Misplaced COMMIT"};

		switch (current.command) {
		case PondRequestCommand::QUERY:
			CommitQuery();
			return BufferedResult::AGAIN;

		case PondRequestCommand::CLONE:
			CommitClone();
			return BufferedResult::AGAIN;

		default:
			throw SimplePondError{"Misplaced COMMIT"};
		}

	case PondRequestCommand::CANCEL:
		AppendListener::Unregister();
		current.Clear();
		socket.UnscheduleWrite();
		break;

	case PondRequestCommand::FILTER_SITE:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_SITE"};

		if (current.HasGroupSite())
			throw SimplePondError{"FILTER_SITE and GROUP_SITE are mutually exclusive"};

		if (HasNullByte(ToStringView(payload)))
			throw SimplePondError{"Malformed FILTER_SITE"};

		{
			auto e = current.filter.sites.emplace(ToStringView(payload));
			if (!e.second)
				throw SimplePondError{"Duplicate FILTER_SITE"};
		}

		return BufferedResult::AGAIN;

	case PondRequestCommand::FILTER_SINCE:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_SINCE"};

		if (current.filter.timestamp.HasSince())
			throw SimplePondError{"Duplicate FILTER_SINCE"};

		if (payload.size() != sizeof(uint64_t))
			throw SimplePondError{"Malformed FILTER_SINCE"};

		current.filter.timestamp.since = Net::Log::TimePoint(Net::Log::Duration(FromBE64(*(const uint64_t *)(const void *)payload.data())));
		if (!current.filter.timestamp.HasSince())
			throw SimplePondError{"Malformed FILTER_SINCE"};
		return BufferedResult::AGAIN;

	case PondRequestCommand::FILTER_UNTIL:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_UNTIL"};

		if (current.filter.timestamp.HasUntil())
			throw SimplePondError{"Duplicate FILTER_UNTIL"};

		if (payload.size() != sizeof(uint64_t))
			throw SimplePondError{"Malformed FILTER_UNTIL"};

		current.filter.timestamp.until = Net::Log::TimePoint(Net::Log::Duration(FromBE64(*(const uint64_t *)(const void *)payload.data())));
		if (!current.filter.timestamp.HasUntil())
			throw SimplePondError{"Malformed FILTER_UNTIL"};
		return BufferedResult::AGAIN;

	case PondRequestCommand::FILTER_TYPE:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_TYPE"};

		if (current.filter.type != Net::Log::Type::UNSPECIFIED)
			throw SimplePondError{"Duplicate FILTER_TYPE"};

		if (payload.size() != sizeof(Net::Log::Type))
			throw SimplePondError{"Malformed FILTER_TYPE"};

		current.filter.type = *(const Net::Log::Type *)(const void *)payload.data();
		if (current.filter.type == Net::Log::Type::UNSPECIFIED)
			throw SimplePondError{"Malformed FILTER_TYPE"};
		return BufferedResult::AGAIN;

	case PondRequestCommand::FOLLOW:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY ||
		    current.continue_)
			throw SimplePondError{"Misplaced FOLLOW"};

		if (current.follow)
			throw SimplePondError{"Duplicate FOLLOW"};

		if (current.HasGroupSite())
			throw SimplePondError{"FOLLOW and GROUP_SITE are mutually exclusive"};

		if (current.HasWindow())
			throw SimplePondError{"FOLLOW and WINDOW are mutually exclusive"};

		if (!payload.empty())
			throw SimplePondError{"Malformed FOLLOW"};

		current.follow = true;
		return BufferedResult::AGAIN;

	case PondRequestCommand::GROUP_SITE:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced GROUP_SITE"};

		if (!current.filter.sites.empty())
			throw SimplePondError{"FILTER_SITE and GROUP_SITE are mutually exclusive"};

		if (current.follow || current.continue_)
			throw SimplePondError{"FOLLOW/CONTINUE and GROUP_SITE are mutually exclusive"};

		if (current.group_site.max_sites > 0)
			throw SimplePondError{"Duplicate GROUP_SITE"};

		if (payload.size() != sizeof(current.group_site))
			throw SimplePondError{"Malformed GROUP_SITE"};

		{
			const auto &src =
				*(const PondGroupSitePayload *)(const void *)payload.data();
			current.group_site.max_sites = FromBE32(src.max_sites);
			current.group_site.skip_sites = FromBE32(src.skip_sites);

			if (current.group_site.max_sites <= 0)
				throw SimplePondError{"Malformed GROUP_SITE"};
		}

		return BufferedResult::AGAIN;

	case PondRequestCommand::CLONE:
		// TODO: check if client is privileged

		if (!IsNonEmptyString(payload))
			throw SimplePondError{"Malformed CLONE"};

		socket.UnscheduleWrite();
		AppendListener::Unregister();
		current.Set(id, cmd);
		current.address.assign(ToStringView(payload));
		return BufferedResult::AGAIN;

	case PondRequestCommand::INJECT_LOG_RECORD:
		// TODO: check if client is privileged

		if (instance.IsBlocked())
			throw SimplePondError{"Blocked"};

		try {
			instance.GetDatabase().Emplace(payload);
		} catch (Net::Log::ProtocolError) {
		}

		return BufferedResult::AGAIN;

	case PondRequestCommand::STATS:
		{
			const auto p = instance.GetStats();
			Send(id, PondResponseCommand::STATS,
			     ReferenceAsBytes(p));
		}

		return BufferedResult::AGAIN;

	case PondRequestCommand::WINDOW:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced WINDOW"};

		if (current.follow || current.continue_)
			throw SimplePondError{"FOLLOW/CONTINUE and WINDOW are mutually exclusive"};

		if (current.HasWindow())
			throw SimplePondError{"Duplicate WINDOW"};

		if (payload.size() != sizeof(current.window))
			throw SimplePondError{"Malformed WINDOW"};

		{
			const auto &src =
				*(const PondWindowPayload *)(const void *)payload.data();
			current.window.max = FromBE64(src.max);
			current.window.skip = FromBE64(src.skip);

			if (current.window.max <= 0)
				throw SimplePondError{"Malformed WINDOW"};
		}

		return BufferedResult::AGAIN;

	case PondRequestCommand::CANCEL_OPERATION:
		instance.CancelBlockingOperation();
		return BufferedResult::AGAIN;

	case PondRequestCommand::FILTER_HTTP_STATUS:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_HTTP_STATUS"};

		{
			const auto &src =
				*(const PondFilterHttpStatusPayload *)(const void *)payload.data();
			if (payload.size() != sizeof(src))
				throw SimplePondError{"Malformed FILTER_HTTP_STATUS"};

			current.filter.http_status.begin = FromBE16(src.begin);
			current.filter.http_status.end = FromBE16(src.end);
		}

		return BufferedResult::AGAIN;

	case PondRequestCommand::FILTER_HTTP_URI_STARTS_WITH:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_HTTP_URI_STARTS_WITH"};

		if (payload.empty() || HasNullByte(ToStringView(payload)))
			throw SimplePondError{"Malformed FILTER_HTTP_STATUS"};

		current.filter.http_uri_starts_with.assign(ToStringView(payload));
		return BufferedResult::AGAIN;

	case PondRequestCommand::FILTER_HOST:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_HOST"};

		if (HasNullByte(ToStringView(payload)))
			throw SimplePondError{"Malformed FILTER_HOST"};

		if (auto e = current.filter.hosts.emplace(ToStringView(payload));
		    !e.second)
			throw SimplePondError{"Duplicate FILTER_HOST"};

		return BufferedResult::AGAIN;

	case PondRequestCommand::FILTER_GENERATOR:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_GENERATOR"};

		if (HasNullByte(ToStringView(payload)))
			throw SimplePondError{"Malformed FILTER_GENERATOR"};

		if (auto e = current.filter.generators.emplace(ToStringView(payload));
		    !e.second)
			throw SimplePondError{"Duplicate FILTER_GENERATOR"};

		return BufferedResult::AGAIN;

	case PondRequestCommand::FILTER_DURATION_LONGER:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_DURATION_LONGER"};

		if (current.filter.duration.HasLonger())
			throw SimplePondError{"Duplicate FILTER_DURATION_LONGER"};

		if (payload.size() != sizeof(uint64_t))
			throw SimplePondError{"Malformed FILTER_DURATION_LONGER"};

		current.filter.duration.longer = Net::Log::Duration{FromBE64(*(const uint64_t *)(const void *)payload.data())};
		if (!current.filter.duration.HasLonger())
			throw SimplePondError{"Malformed FILTER_DURATION_LONGER"};
		return BufferedResult::AGAIN;

	case PondRequestCommand::CONTINUE:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY ||
		    current.follow)
			throw SimplePondError{"Misplaced CONTINUE"};

		if (current.continue_)
			throw SimplePondError{"Duplicate CONTINUE"};

		if (current.HasGroupSite())
			throw SimplePondError{"CONTINUE and GROUP_SITE are mutually exclusive"};

		if (current.HasWindow())
			throw SimplePondError{"CONTINUE and WINDOW are mutually exclusive"};

		if (!payload.empty())
			throw SimplePondError{"Malformed CONTINUE"};

		current.continue_ = true;
		return BufferedResult::AGAIN;
	}

	throw SimplePondError{"Command not implemented"};
} catch (SimplePondError e) {
	Send(id, PondResponseCommand::ERROR, AsBytes(e.message));
	AppendListener::Unregister();
	current.Clear();
	socket.UnscheduleWrite();
	return BufferedResult::AGAIN;
}

BufferedResult
Connection::OnBufferedData()
{
	auto r = socket.ReadBuffer();
	if (r.size() < sizeof(PondHeader))
		return BufferedResult::MORE;

	const auto *be_header = (const PondHeader *)(const void *)r.data();

	const size_t payload_size = FromBE16(be_header->size);
	if (r.size() < sizeof(PondHeader) + payload_size)
		return BufferedResult::MORE;

	const uint16_t id = FromBE16(be_header->id);
	const auto command = PondRequestCommand(FromBE16(be_header->command));

	size_t consumed = sizeof(*be_header) + payload_size;
	socket.KeepConsumed(consumed);

	return OnPacket(id, command, {(const std::byte *)(be_header + 1), payload_size});
}

bool
Connection::OnBufferedClosed() noexcept
{
	Destroy();
	return false;
}

/**
 * @param queue push remaining data of a short send to this queue
 */
static size_t
SendMulti(SocketDescriptor s, uint16_t id,
	  Selection selection, uint64_t max_records,
	  SendQueue &queue)
{
	constexpr size_t CAPACITY = 256;

	std::array<PondIovec, CAPACITY> vecs;
	std::array<struct mmsghdr, CAPACITY> msgs;

	if (max_records > CAPACITY)
		max_records = CAPACITY;

	size_t n = 0;
	do {
		const auto &record = *selection;
		auto &m = msgs[n].msg_hdr;
		auto &v = vecs[n];

		m.msg_name = nullptr;
		m.msg_namelen = 0;
		m.msg_iovlen = MakeIovec(v, id,
					 PondResponseCommand::LOG_RECORD,
					 record.GetRaw());
		m.msg_iov = v.vec.data();
		m.msg_control = nullptr;
		m.msg_controllen = 0;
		m.msg_flags = 0;

		++n;
		++selection;
	} while (selection && n < max_records);

	int result = sendmmsg(s.Get(), msgs.data(), n,
			      MSG_DONTWAIT|MSG_NOSIGNAL);
	if (result < 0) {
		const auto e = GetSocketError();
		if (IsSocketErrorSendWouldBlock(e))
			return 0;
		throw MakeSocketError(e, "Failed to send");
	}

	if (result > 0)
		/* if the last send was short, enqueue the remaining
		   data */
		vecs[result - 1].Queue(queue, msgs[result - 1].msg_len);

	return result;
}

bool
Connection::OnBufferedWrite()
{
	if (!send_queue.empty()) {
		if (!send_queue.Flush(GetSocket()))
			/* still not empty, try again in the next
			   call */
			return true;

		if (!current.IsDefined()) {
			/* the response was already finished (when the
			   send_queue was filled), and there's nothing
			   left to do */
			socket.UnscheduleWrite();
			return true;
		}
	}

	assert(current.command == PondRequestCommand::QUERY);
	assert(current.selection);

	auto &selection = *current.selection;
	selection.FixDeleted();

	/* handle window.skip */
	uint64_t max_records = std::numeric_limits<uint64_t>::max();
	if (current.HasWindow()) {
		unsigned n_skipped = 0;
		while (selection && current.window.skip > 0) {
			if (++n_skipped > 1024 * 1024)
				/* yield to avoid DoS by a huge number
				   of skips */
				return true;

			++selection;
			--current.window.skip;
		}

		max_records = current.window.max;
	}

	if (selection) {
		size_t n = SendMulti(GetSocket(), current.id,
				     selection, max_records,
				     send_queue);

		if (current.HasWindow()) {
			current.window.max -= n;
			if (current.window.max == 0) {
				Send(current.id, PondResponseCommand::END,
				     {});
				assert(!AppendListener::IsRegistered());
				current.Clear();
				if (send_queue.empty())
					socket.UnscheduleWrite();
				return true;
			}
		}

		selection += n;
		if (selection)
			return true;
	}

	if (current.site_iterator != nullptr &&
	    --current.group_site.max_sites > 0) {
		auto &db = instance.GetDatabase();
		auto next = db.GetNextSite(*current.site_iterator);
		current.site_iterator = FindNonEmpty(db, current.filter, next);
		// TODO: max_sites
		if (current.site_iterator != nullptr) {
			/* we have another site; return from this
			   method, leaving the "write" scheduled, so
			   we'll be called again and this next call
			   will send the new site's data */
			selection = db.Select(*current.site_iterator,
					      current.filter);
			return true;
		}

		/* no more sites, end this response */
	}

	if (current.follow || current.continue_) {
		current.selection->AddAppendListener(*this);
	} else {
		Send(current.id, PondResponseCommand::END, {});
		assert(!AppendListener::IsRegistered());
		current.Clear();
	}

	if (send_queue.empty())
		socket.UnscheduleWrite();
	return true;
}

void
Connection::OnBufferedError(std::exception_ptr e) noexcept
{
	logger(2, e);
	Destroy();
}

bool
Connection::OnAppend(const Record &record) noexcept
{
	assert(current.command == PondRequestCommand::QUERY);
	assert(current.selection);

	auto &selection = *current.selection;
	assert(!selection);

	if (!selection.OnAppend(record)) {
		/* no matching record was appended: keep the
		   AppendListener registered */
		assert(!selection);
		return true;
	}

	/* a matching record was appended: unregister the
	   AppendListener and prepare for sending the record to our
	   client */
	assert(selection);
	socket.ScheduleWrite();
	return false;
}
