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

#include "Instance.hxx"
#include "event/net/UdpListener.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketConfig.hxx"
#include "net/log/Parser.hxx"
#include "util/PrintException.hxx"

bool
Instance::OnUdpDatagram(ConstBuffer<void> payload,
			WritableBuffer<UniqueFileDescriptor> fds,
			SocketAddress address, int uid)
{
	(void)fds;
	(void)address;
	(void)uid;

	++n_received;

	if (payload.size == MAX_DATAGRAM_SIZE) {
		/* this datagram was probably truncated, so don't
		   bother parsing it */
		++n_malformed;
		return true;
	}

	try {
		const auto *r =
			database.CheckEmplace(ConstBuffer<uint8_t>::FromVoid(payload),
					      event_loop.GetSteadyClockCache());
		if (r == nullptr)
			++n_discarded;
	} catch (Net::Log::ProtocolError) {
		++n_malformed;
	}

	MaybeScheduleMaxAgeTimer();

	return true;
}

void
Instance::OnUdpError(std::exception_ptr ep) noexcept
{
	logger(1, "UDP receiver error: ", ep);
}
