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
#include "Error.hxx"
#include "Instance.hxx"
#include "Selection.hxx"
#include "net/log/Parser.hxx"
#include "system/Error.hxx"
#include "util/ByteOrder.hxx"

#include <array>

#include <sys/socket.h>

void
Connection::Request::Clear()
{
	command = PondRequestCommand::NOP;
	filter = Filter();
	group_site.max_sites = 0;
	follow = false;
	selection.reset();
	address.clear();
}

Connection::Connection(Instance &_instance, UniqueSocketDescriptor &&_fd)
	:instance(_instance), logger(instance.GetLogger()),
	 socket(_instance.GetEventLoop())
{
	socket.Init(_fd.Release(), FD_TCP,
		    Event::Duration(-1),
		    std::chrono::seconds(30),
		    *this);
	socket.DeferRead(false);
}

Connection::~Connection()
{
	if (socket.IsConnected())
		socket.Close();

	socket.Destroy();
}

bool
Connection::IsLocalAdmin() const noexcept
{
	const auto cred = socket.GetSocket().GetPeerCredentials();
	return cred.pid != -1 && (cred.uid == 0 || cred.uid == geteuid());
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

template<typename T, typename I>
static inline void
EmplaceMove(std::set<T> &dest, const I begin, const I end)
{
	for (auto i = begin; i != end; ++i)
		dest.emplace(std::move(*i));
}

template<typename T>
static inline std::set<T>
VectorWindow(std::vector<T> &&v, size_t offset, size_t count) noexcept
{
	std::set<T> result;
	if (offset < v.size()) {
		size_t end_offset = offset + count;
		if (end_offset > v.size())
			end_offset = v.size();

		EmplaceMove(result, std::next(v.begin(), offset),
			    std::next(v.begin(), end_offset));
	}

	return result;
}

inline void
Connection::CommitQuery()
{
	auto &db = instance.GetDatabase();

	if (current.follow) {
		current.selection.reset(new Selection(db.Follow(current.filter, *this)));
	} else if (current.HasGroupSite()) {
		// TODO: cache the CollectSites() result, because it's expensive
		current.filter.sites = VectorWindow(db.CollectSites(current.filter),
						    current.group_site.skip_sites,
						    current.group_site.max_sites);

		current.selection.reset(current.filter.sites.empty()
					? new Selection(AnyRecordList(), Filter())
					: new Selection(db.Select(current.filter)));
		socket.ScheduleWrite();
	} else {
		current.selection.reset(new Selection(db.Select(current.filter)));
		socket.ScheduleWrite();
	}

	/* the response will be assembled by
	   OnBufferedWrite() */
}

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
		AppendListener::Unregister();
		current.Set(id, cmd);
		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::COMMIT:
		if (!current.MatchId(id))
			throw SimplePondError{"Misplaced COMMIT"};

		switch (current.command) {
		case PondRequestCommand::QUERY:
			CommitQuery();
			return BufferedResult::AGAIN_OPTIONAL;

		case PondRequestCommand::CLONE:
			CommitClone();
			return BufferedResult::AGAIN_OPTIONAL;

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

		if (!IsNonEmptyString(payload))
			throw SimplePondError{"Malformed FILTER_SITE"};

		{
			auto e = current.filter.sites.emplace((const char *)payload.data,
							      payload.size);
			if (!e.second)
				throw SimplePondError{"Duplicate FILTER_SITE"};
		}

		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::FILTER_SINCE:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_SINCE"};

		if (current.filter.since != Net::Log::TimePoint::min())
			throw SimplePondError{"Duplicate FILTER_SINCE"};

		if (payload.size != sizeof(uint64_t))
			throw SimplePondError{"Malformed FILTER_SINCE"};

		current.filter.since = Net::Log::TimePoint(Net::Log::Duration(FromBE64(*(const uint64_t *)payload.data)));
		if (current.filter.since == Net::Log::TimePoint::min())
			throw SimplePondError{"Malformed FILTER_SINCE"};
		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::FILTER_UNTIL:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_UNTIL"};

		if (current.filter.until != Net::Log::TimePoint::max())
			throw SimplePondError{"Duplicate FILTER_UNTIL"};

		if (payload.size != sizeof(uint64_t))
			throw SimplePondError{"Malformed FILTER_UNTIL"};

		current.filter.until = Net::Log::TimePoint(Net::Log::Duration(FromBE64(*(const uint64_t *)payload.data)));
		if (current.filter.until == Net::Log::TimePoint::max())
			throw SimplePondError{"Malformed FILTER_UNTIL"};
		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::FILTER_TYPE:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FILTER_TYPE"};

		if (current.filter.type != Net::Log::Type::UNSPECIFIED)
			throw SimplePondError{"Duplicate FILTER_TYPE"};

		if (payload.size != sizeof(Net::Log::Type))
			throw SimplePondError{"Malformed FILTER_TYPE"};

		current.filter.type = *(const Net::Log::Type *)payload.data;
		if (current.filter.type == Net::Log::Type::UNSPECIFIED)
			throw SimplePondError{"Malformed FILTER_TYPE"};
		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::FOLLOW:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced FOLLOW"};

		if (current.follow)
			throw SimplePondError{"Duplicate FOLLOW"};

		if (current.HasGroupSite())
			throw SimplePondError{"FOLLOW and GROUP_SITE are mutually exclusive"};

		if (!payload.empty())
			throw SimplePondError{"Malformed FOLLOW"};

		current.follow = true;
		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::GROUP_SITE:
		if (!current.MatchId(id) ||
		    current.command != PondRequestCommand::QUERY)
			throw SimplePondError{"Misplaced GROUP_SITE"};

		if (!current.filter.sites.empty())
			throw SimplePondError{"FILTER_SITE and GROUP_SITE are mutually exclusive"};

		if (current.follow)
			throw SimplePondError{"FOLLOW and GROUP_SITE are mutually exclusive"};

		if (current.group_site.max_sites > 0)
			throw SimplePondError{"Duplicate GROUP_SITE"};

		if (payload.size != sizeof(current.group_site))
			throw SimplePondError{"Malformed GROUP_SITE"};

		{
			const auto &src =
				*(const PondGroupSitePayload *)payload.data;
			current.group_site.max_sites = FromBE32(src.max_sites);
			current.group_site.skip_sites = FromBE32(src.skip_sites);

			if (current.group_site.max_sites <= 0)
				throw SimplePondError{"Malformed GROUP_SITE"};
		}

		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::CLONE:
		// TODO: check if client is privileged

		if (!IsNonEmptyString(payload))
			throw SimplePondError{"Malformed CLONE"};

		socket.UnscheduleWrite();
		AppendListener::Unregister();
		current.Set(id, cmd);
		current.address.assign((const char *)payload.data,
				       payload.size);
		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::INJECT_LOG_RECORD:
		// TODO: check if client is privileged

		try {
			instance.GetDatabase().Emplace({(const uint8_t *)payload.data,
						payload.size});
		} catch (Net::Log::ProtocolError) {
		}

		return BufferedResult::AGAIN_EXPECT;

	case PondRequestCommand::STATS:
		{
			const auto &db = instance.GetDatabase();
			PondStatsPayload p{};
			p.memory_capacity = ToBE64(db.GetMemoryCapacity());
			p.memory_usage = ToBE64(db.GetMemoryUsage());
			p.n_records = ToBE32(db.GetRecordCount());

			Send(id, PondResponseCommand::STATS, {&p, sizeof(p)});
		}

		return BufferedResult::AGAIN_EXPECT;
	}

	throw SimplePondError{"Command not implemented"};
} catch (SimplePondError e) {
	Send(id, PondResponseCommand::ERROR, e.message.ToVoid());
	AppendListener::Unregister();
	current.Clear();
	socket.UnscheduleWrite();
	return BufferedResult::AGAIN_OPTIONAL;
}

BufferedResult
Connection::OnBufferedData()
{
	auto r = socket.ReadBuffer();
	if (r.size < sizeof(PondHeader))
		return BufferedResult::MORE;

	const auto *be_header = (const PondHeader *)r.data;

	const size_t payload_size = FromBE16(be_header->size);
	if (r.size < sizeof(PondHeader) + payload_size)
		return BufferedResult::MORE;

	const uint16_t id = FromBE16(be_header->id);
	const auto command = PondRequestCommand(FromBE16(be_header->command));

	size_t consumed = sizeof(*be_header) + payload_size;
	socket.KeepConsumed(consumed);

	return OnPacket(id, command, {be_header + 1, payload_size});
}

bool
Connection::OnBufferedClosed() noexcept
{
	Destroy();
	return false;
}

static size_t
SendMulti(SocketDescriptor s, uint16_t id,
	  Selection selection)
{
	constexpr size_t CAPACITY = 256;

	std::array<PondIovec, CAPACITY> vecs;
	std::array<struct mmsghdr, CAPACITY> msgs;

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
		m.msg_iov = &v.vec.front();
		m.msg_control = nullptr;
		m.msg_controllen = 0;
		m.msg_flags = 0;

		++n;
		++selection;
	} while (selection && n < CAPACITY);

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
	assert(current.selection);

	auto &selection = *current.selection;
	selection.FixDeleted();

	if (selection) {
		selection += SendMulti(socket.GetSocket(), current.id, selection);
		if (selection)
			return true;
	}

	if (current.follow) {
		current.selection->AddAppendListener(*this);
	} else {
		Send(current.id, PondResponseCommand::END, nullptr);
		assert(!AppendListener::IsRegistered());
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
