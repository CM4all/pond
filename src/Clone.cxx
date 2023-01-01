/*
 * Copyright 2017-2022 CM4all GmbH
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
#include "BlockingOperation.hxx"
#include "Port.hxx"
#include "Error.hxx"
#include "Instance.hxx"
#include "client/Async.hxx"
#include "client/Open.hxx"
#include "client/Datagram.hxx"
#include "net/RConnectSocket.hxx"
#include "net/log/Parser.hxx"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"
#include "util/SpanCast.hxx"

class CloneOperation final : public BlockingOperation, PondAsyncClientHandler {
	const LLogger logger;

	BlockingOperationHandler &handler;

	Database &db;

	PondAsyncClient client;

	const uint16_t id;

	bool pending_clear = true;

public:
	CloneOperation(BlockingOperationHandler &_handler,
		       Database &_db,
		       EventLoop &event_loop,
		       UniqueSocketDescriptor &&socket)
		:logger("clone"), handler(_handler), db(_db),
		 client(event_loop, std::move(socket), *this),
		 id(client.MakeId())
	{
		client.Send(id, PondRequestCommand::QUERY);
		client.Send(id, PondRequestCommand::COMMIT);
	}

private:
	/* virtual methods from class PondAsyncClientHandler */
	bool OnPondDatagram(uint16_t id, PondResponseCommand command,
			    std::span<const std::byte> payload) override;

	void OnPondError(std::exception_ptr e) noexcept override {
		logger(1, "CLONE error: ", e);
		handler.OnOperationFinished();
	}
};

static std::string
ToString(std::span<const std::byte> b) noexcept
{
	return std::string{ToStringView(b)};
}

bool
CloneOperation::OnPondDatagram(uint16_t _id, PondResponseCommand command,
			       std::span<const std::byte> payload)
{
	if (_id != id)
		return true;

	switch (command) {
	case PondResponseCommand::NOP:
		break;

	case PondResponseCommand::ERROR:
		throw std::runtime_error(ToString(payload));

	case PondResponseCommand::END:
		handler.OnOperationFinished();
		return false;

	case PondResponseCommand::LOG_RECORD:
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
		throw std::runtime_error("Unexpected response packet");
	}

	return true;
}

void
Connection::CommitClone()
try {
	if (!IsLocalAdmin())
		throw SimplePondError{"Forbidden"};

	if (instance.IsBlocked())
		throw SimplePondError{"Blocked"};

	instance.SetBlockingOperation(std::make_unique<CloneOperation>(instance,
								       instance.GetDatabase(),
								       GetEventLoop(),
								       ResolveConnectStreamSocket(current.address.c_str(),
												  POND_DEFAULT_PORT)));

	Send(current.id, PondResponseCommand::END, {});
	current.Clear();
} catch (const SimplePondError &e) {
	logger(1, "CLONE error: ", e.message);
	throw;
} catch (...) {
	logger(1, "CLONE error: ", std::current_exception());
	throw SimplePondError{"CLONE error"};
}
