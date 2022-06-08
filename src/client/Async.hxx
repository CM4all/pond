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

#pragma once

#include "Protocol.hxx"
#include "Send.hxx"
#include "event/SocketEvent.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/StaticFifoBuffer.hxx"

/**
 * Handler fro #PondAsyncClient.
 */
class PondAsyncClientHandler {
public:
	/**
	 * Exceptions thrown by this method will be passed on OnPondError().
	 *
	 * @return if the #PondAsyncClient has been destroyed
	 */
	virtual bool OnPondDatagram(uint16_t id, PondResponseCommand command,
				    std::span<const std::byte> payload) = 0;

	virtual void OnPondError(std::exception_ptr e) noexcept = 0;
};

/**
 * Non-blocking version of #PondClient which integrates with an
 * #EventLoop.
 */
class PondAsyncClient {
	PondAsyncClientHandler &handler;

	SocketEvent s;

	uint16_t last_id = 0;

	StaticFifoBuffer<std::byte, 256 * 1024> input;

public:
	PondAsyncClient(EventLoop &event_loop,
			UniqueSocketDescriptor &&_s,
			PondAsyncClientHandler &_handler) noexcept
		:handler(_handler),
		 s(event_loop, BIND_THIS_METHOD(OnSocketReady), _s.Release())
	{
		s.ScheduleRead();
	}

	~PondAsyncClient() noexcept {
		s.Close();
	}

	SocketDescriptor GetSocket() {
		return s.GetSocket();
	}

	uint16_t MakeId() {
		return ++last_id;
	}

	void Send(uint16_t id, PondRequestCommand command) {
		SendPondRequest(GetSocket(), id, command);
	}

	template<typename T>
	void Send(uint16_t id, PondRequestCommand command, T &&payload) {
		SendPondRequest(GetSocket(), id, command,
				std::forward<T>(payload));
	}

	template<typename T>
	void SendT(uint16_t id, PondRequestCommand command,
		   const T &payload) {
		SendPondRequestT(GetSocket(), id, command, payload);
	}

	bool IsEmpty() const noexcept {
		return input.empty();
	}

private:
	void FillInputBuffer();

	void OnSocketReady(unsigned events) noexcept;
};
