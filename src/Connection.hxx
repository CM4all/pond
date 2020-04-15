/*
 * Copyright 2017-2020 CM4all GmbH
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

#pragma once

#include "Protocol.hxx"
#include "Filter.hxx"
#include "AppendListener.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "event/net/BufferedSocket.hxx"

#include <boost/intrusive/list_hook.hpp>

#include <memory>
#include <string>

#include <stdint.h>

class Instance;
class Selection;
class SiteIterator;
class RootLogger;
template<typename t> struct ConstBuffer;

class Connection final
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>,
	BufferedSocketHandler, AppendListener {

	Instance &instance;
	const RootLogger &logger;

	BufferedSocket socket;

	struct Request {
		uint16_t id = 0;
		bool follow = false;
		PondRequestCommand command = PondRequestCommand::NOP;

		Filter filter;

		PondGroupSitePayload group_site;

		PondWindowPayload window;

		std::unique_ptr<Selection> selection;

		/**
		 * The current site being iterated in GROUP_SITE mode.
		 */
		SiteIterator *site_iterator;

		std::string address;

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

		bool HasGroupSite() const noexcept {
			return group_site.max_sites > 0;
		}

		bool HasWindow() const noexcept {
			return window.max > 0;
		}

		void Clear();

		void Set(uint16_t _id,
			 PondRequestCommand _command) {
			Clear();
			id = _id;
			command = _command;
		}
	} current;

public:
	Connection(Instance &_instance, UniqueSocketDescriptor &&fd);
	~Connection();

	void Destroy() {
		delete this;
	}

	auto &GetEventLoop() const noexcept {
		return socket.GetEventLoop();
	}

private:
	bool IsLocalAdmin() const noexcept;

	void Send(uint16_t id, PondResponseCommand command,
		  ConstBuffer<void> payload);

	void CommitQuery();
	void CommitClone();

	BufferedResult OnPacket(uint16_t id, PondRequestCommand cmd,
				ConstBuffer<void> payload);

	void OnAppend() noexcept;

	/* virtual methods from class BufferedSocketHandler */
	BufferedResult OnBufferedData() override;
	bool OnBufferedClosed() noexcept override;
	bool OnBufferedWrite() override;
	void OnBufferedError(std::exception_ptr e) noexcept override;

	/* virtual methods from class AppendListener */
	bool OnAppend(const Record &record) noexcept override;
};
