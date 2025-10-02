// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "AutoClone.hxx"
#include "Config.hxx"
#include "Database.hxx"
#include "client/Async.hxx"
#include "lib/avahi/Client.hxx"
#include "event/net/ConnectSocket.hxx"
#include "net/log/Parser.hxx"
#include "lib/fmt/SocketError.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/SpanCast.hxx"

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <memory>

#include <net/if.h>

static AvahiIfIndex
GetAvahiIfIndex(const ListenerConfig &listener)
{
	const char *name;
	if (!listener.zeroconf.interface.empty())
		name = listener.zeroconf.interface.c_str();
	else if (!listener.interface.empty())
		name = listener.interface.c_str();
	else
		return AVAHI_IF_UNSPEC;

	int i = if_nametoindex(name);
	if (i == 0)
		throw FmtSocketError("Failed to find interface {:?}",
				     listener.interface);

	return AvahiIfIndex(i);
}

class AutoCloneOperation::Server final
	: public AutoUnlinkIntrusiveListHook,
	  ConnectSocketHandler, PondAsyncClientHandler
{
	LLogger logger;

	AutoCloneOperation &operation;
	Database &db;

	const std::string key;

	ConnectSocket connect;

	std::unique_ptr<PondAsyncClient> client;

	enum class State {
		NONE,
		CONNECT,
		STATS,
		IDLE,
		CLONE,
	} state = State::NONE;

	uint16_t id;

	bool pending_clear;

	uint64_t n_records;

public:
	Server(AutoCloneOperation &_operation, Database &_db,
	       const std::string &_key) noexcept
		:logger("auto_clone"),
		 operation(_operation), db(_db), key(_key),
		 connect(operation.GetEventLoop(), *this)
	{
	}

	auto &GetEventLoop() noexcept {
		return connect.GetEventLoop();
	}

	void Connect(SocketAddress address) noexcept {
		assert(state == State::NONE);

		state = State::CONNECT;
		connect.Connect(address, std::chrono::seconds(5));
	}

	void Clone() noexcept {
		assert(state == State::IDLE);

		state = State::CLONE;
		pending_clear = true;

		id = client->MakeId();

		try {
			client->Send(id, PondRequestCommand::QUERY);
			client->Send(id, PondRequestCommand::COMMIT);
		} catch (...) {
			operation.OnServerError(*this,
						std::current_exception());
		}
	}

	const auto &GetKey() const noexcept {
		return key;
	}

	bool IsIdle() const noexcept {
		return state == State::IDLE;
	}

	uint64_t GetRecordCount() const noexcept {
		assert(state == State::IDLE);

		return n_records;
	}

private:
	/* virtual methods from class ConnectSocketHandler */
	void OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept override {
		assert(state == State::CONNECT);
		state = State::STATS;

		PondAsyncClientHandler &client_handler = *this;

		client = std::make_unique<PondAsyncClient>(GetEventLoop(),
							   std::move(fd),
							   client_handler);
		id = client->MakeId();

		try {
			client->Send(id, PondRequestCommand::STATS);
		} catch (...) {
			operation.OnServerError(*this,
						std::current_exception());
			return;
		}
	}

	void OnSocketConnectError(std::exception_ptr ep) noexcept override {
		assert(state == State::CONNECT);

		operation.OnServerError(*this, std::move(ep));
	}

	/* virtual methods from class PondAsyncClientHandler */
	bool OnPondDatagram(uint16_t id, PondResponseCommand command,
			    std::span<const std::byte> payload) override;

	void OnPondError(std::exception_ptr e) noexcept override {
		assert(state >= State::STATS);

		operation.OnServerError(*this, std::move(e));
	}
};

static std::string
ToString(std::span<const std::byte> b) noexcept
{
	return std::string{ToStringView(b)};
}

bool
AutoCloneOperation::Server::OnPondDatagram(uint16_t _id,
					   PondResponseCommand command,
					   std::span<const std::byte> payload)
{
	assert(state >= State::STATS);

	if (_id != id)
		return true;

	switch (command) {
	case PondResponseCommand::NOP:
		break;

	case PondResponseCommand::ERROR:
		throw std::runtime_error(ToString(payload));

	case PondResponseCommand::END:
		if (state == State::CLONE) {
			operation.OnServerFinished(*this);
			return false;
		}

		return true;

	case PondResponseCommand::LOG_RECORD:
		if (state != State::CLONE)
			break;

		if (pending_clear) {
			/* postpone the Clear() call until we have
			   received at least one datagram */
			pending_clear = false;
			db.Clear();
		}

		try {
			db.Emplace(payload);
		} catch (const Net::Log::ProtocolError &) {
			logger(3, "Failed to parse datagram during CLONE: ",
			       std::current_exception());
		}
		break;

	case PondResponseCommand::STATS:
		if (state == State::STATS) {
			const auto &stats = *(const PondStatsPayload *)(const void *)payload.data();
			if (payload.size() < sizeof(stats))
				throw std::runtime_error("Malformed STATS packet");

			n_records = FromBE64(stats.n_records);

			state = State::IDLE;
			operation.OnServerStats(*this);
		} else
			throw std::runtime_error("Unexpected response packet");
	}

	return true;
}

AutoCloneOperation::AutoCloneOperation(BlockingOperationHandler &_handler,
				       Database &_db,
				       Avahi::Client &avahi_client,
				       const ListenerConfig &listener) noexcept
	:logger("auto_clone"), handler(_handler),
	 db(_db),
	 explorer(avahi_client, *this,
		  GetAvahiIfIndex(listener),
		  listener.zeroconf.protocol,
		  listener.zeroconf.service.c_str(),
		  nullptr,
		  *this),
	 timeout_event(avahi_client.GetEventLoop(),
		       BIND_THIS_METHOD(OnTimeout))
{
	timeout_event.Schedule(std::chrono::seconds{90});

#ifdef HAVE_LIBSYSTEMD
	sd_notify(0, "STATUS=Initiating auto_clone");
#endif
}

AutoCloneOperation::~AutoCloneOperation() noexcept
{
	servers.clear_and_dispose(DeleteDisposer{});
}

void
AutoCloneOperation::OnTimeout() noexcept
{
	/* remove servers which have not yet received any stats */
	servers.remove_and_dispose_if([](auto &i) { return !i.IsIdle(); },
				      DeleteDisposer{});

	/* find the best available server */
	Server *best = nullptr;
	for (auto &i : servers) {
		logger(1, "Found server '", i.GetKey(), "' with ",
		       i.GetRecordCount(), " records");

		if (best == nullptr ||
		    i.GetRecordCount() > best->GetRecordCount())
			best = &i;
	}

	if (best == nullptr) {
		logger(1, "No server found for auto_clone");
		handler.OnOperationFinished();
		return;
	}

	logger(1, "Cloning from ", best->GetKey());

#ifdef HAVE_LIBSYSTEMD
	sd_notifyf(0, "STATUS=Cloning from %s", best->GetKey().c_str());
#endif

	/* remove all other servers */
	servers.remove_and_dispose_if([best](auto &i) { return &i != best; },
				      DeleteDisposer{});

	/* start the clone */
	best->Clone();
}

void
AutoCloneOperation::OnServerStats(Server &) noexcept
{
}

void
AutoCloneOperation::OnServerFinished(Server &) noexcept
{
	logger(1, "Finished auto_clone");
#ifdef HAVE_LIBSYSTEMD
	sd_notify(0, "STATUS=");
#endif

	handler.OnOperationFinished();
}

void
AutoCloneOperation::OnServerError(Server &server,
				  std::exception_ptr e) noexcept
{
	logger(2, "Server '", server.GetKey(), "' failed: ", e);
#ifdef HAVE_LIBSYSTEMD
	sd_notify(0, "STATUS=");
#endif

	delete &server;

	if (!timeout_event.IsPending()) {
		/* we were already cloning, so this is the final
		   failure */
		assert(servers.empty());

		handler.OnOperationFinished();
	}
}

void
AutoCloneOperation::OnAvahiNewObject(const std::string &key,
				     SocketAddress address,
				     [[maybe_unused]] AvahiStringList *txt) noexcept
{
	if (servers.empty())
		/* the first server was just found; reduce the timeout
		   to 5 seconds to find more servers */
		timeout_event.ScheduleEarlier(std::chrono::seconds{5});

	auto *server = new Server(*this, db, key);
	servers.push_back(*server);
	server->Connect(address);
}

bool
AutoCloneOperation::OnAvahiError(std::exception_ptr e) noexcept
{
	logger(2, e);
	return false;
}
