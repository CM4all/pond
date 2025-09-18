// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Protocol.hxx"
#include "Filter.hxx"
#include "AppendListener.hxx"
#include "SendQueue.hxx"
#include "SiteIterator.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "event/net/BufferedSocket.hxx"
#include "util/IntrusiveList.hxx"

#include <memory>
#include <span>
#include <string>

#include <stdint.h>

class Instance;
class Selection;
class SiteIterator;
class RootLogger;
class Connection final
	: public AutoUnlinkIntrusiveListHook,
	BufferedSocketHandler, AppendListener {

	Instance &instance;
	const RootLogger &logger;

	BufferedSocket socket;

	SendQueue send_queue;

	struct Request {
		uint16_t id = 0;
		bool follow = false, continue_ = false;
		PondRequestCommand command = PondRequestCommand::NOP;

		Filter filter;

		PondGroupSitePayload group_site;

		PondWindowPayload window;

		std::unique_ptr<Selection> selection;

		/**
		 * The current site being iterated in GROUP_SITE mode.
		 */
		SiteIterator site_iterator;

		std::string address;

		constexpr bool IsDefined() const noexcept {
			return command != PondRequestCommand::NOP;
		}

		/**
		 * Does the given request id belong to this
		 * uncommitted request?
		 */
		constexpr bool MatchId(uint16_t other_id) const noexcept {
			return IsDefined() && id == other_id;
		}

		/**
		 * Shall this request id be ignored, because it has
		 * already aborted?  This can happen if one request
		 * command fails, but more incremental request packets
		 * are still in the socket buffer.
		 */
		constexpr bool IgnoreId(uint16_t other_id) const noexcept {
			return !IsDefined() && id == other_id;
		}

		constexpr bool HasGroupSite() const noexcept {
			return group_site.max_sites > 0;
		}

		constexpr bool HasWindow() const noexcept {
			return window.max > 0;
		}

		void Clear() noexcept;

		void Set(uint16_t _id,
			 PondRequestCommand _command) noexcept {
			Clear();
			id = _id;
			command = _command;
		}
	} current;

public:
	Connection(Instance &_instance, UniqueSocketDescriptor &&fd) noexcept;
	~Connection() noexcept;

	void Destroy() noexcept {
		delete this;
	}

	auto &GetEventLoop() const noexcept {
		return socket.GetEventLoop();
	}

private:
	SocketDescriptor GetSocket() const noexcept {
		return socket.GetSocket();
	}

	bool IsLocalAdmin() const noexcept;

	void Send(uint16_t id, PondResponseCommand command,
		  std::span<const std::byte> payload);

	void CommitQuery() noexcept;
	void CommitClone();

	BufferedResult OnPacket(uint16_t id, PondRequestCommand cmd,
				std::span<const std::byte> payload);

	void OnAppend() noexcept;

	/* virtual methods from class BufferedSocketHandler */
	BufferedResult OnBufferedData() override;
	bool OnBufferedClosed() noexcept override;
	bool OnBufferedWrite() override;
	void OnBufferedError(std::exception_ptr e) noexcept override;

	/* virtual methods from class AppendListener */
	bool OnAppend(const Record &record) noexcept override;
};
