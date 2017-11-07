/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Protocol.hxx"
#include "Filter.hxx"
#include "Cursor.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "event/net/BufferedSocket.hxx"

#include <boost/intrusive/list_hook.hpp>

#include <string>

#include <stdint.h>

class Instance;
class RootLogger;
template<typename t> struct ConstBuffer;

class Connection final
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>,
	  BufferedSocketHandler {

	Instance &instance;
	const RootLogger &logger;

	BufferedSocket socket;

	struct Request {
		uint16_t id = 0;
		bool follow = false;
		PondRequestCommand command = PondRequestCommand::NOP;

		Filter filter;

		Cursor cursor;

		Request(Database &database, BoundMethod<void()> push_callback)
			:cursor(database, push_callback) {}

		bool IsDefined() const {
			return command != PondRequestCommand::NOP;
		}

		/**
		 * Does the given request id belong to this
		 * uncommitted request?
		 */
		bool MatchId(uint16_t other_id) const {
			return IsDefined() && id == other_id;
		}

		/**
		 * Shall this request id be ignored, because it has
		 * already aborted?  This can happen if one request
		 * command fails, but more incremental request packets
		 * are still in the socket buffer.
		 */
		bool IgnoreId(uint16_t other_id) const {
			return !IsDefined() && id == other_id;
		}

		void Clear() {
			command = PondRequestCommand::NOP;
			filter = Filter();
			follow = false;
			cursor.clear();
		}

		void Set(uint16_t _id,
			 PondRequestCommand _command) {
			id = _id;
			command = _command;
			filter = Filter();
			follow = false;
			cursor.clear();
		}
	} current;

public:
	Connection(Instance &_instance, UniqueSocketDescriptor &&fd);
	~Connection();

	void Destroy() {
		delete this;
	}

private:
	void Send(uint16_t id, PondResponseCommand command,
		  ConstBuffer<void> payload);

	BufferedResult OnPacket(uint16_t id, PondRequestCommand cmd,
				ConstBuffer<void> payload);

	void OnAppend();

	/* virtual methods from class BufferedSocketHandler */
	BufferedResult OnBufferedData(const void *buffer, size_t size) override;
	bool OnBufferedClosed() noexcept override;
	bool OnBufferedWrite() override;
	void OnBufferedError(std::exception_ptr e) noexcept override;
};
