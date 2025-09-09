// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "event/net/UdpListener.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketConfig.hxx"
#include "net/log/Parser.hxx"
#include "util/PrintException.hxx"

bool
Instance::OnUdpDatagram(std::span<const std::byte> payload,
			std::span<UniqueFileDescriptor> fds,
			SocketAddress address, int uid)
{
	(void)fds;
	(void)address;
	(void)uid;

	if (IsBlocked())
		/* ignore incoming datagrams while the CLONE runs */
		return true;

	++n_received;

	if (payload.size() == MAX_DATAGRAM_SIZE) {
		/* this datagram was probably truncated, so don't
		   bother parsing it */
		++n_malformed;
		return true;
	}

	try {
		const auto *r =
			database.CheckEmplace(payload,
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
Instance::OnUdpError(std::exception_ptr &&error) noexcept
{
	logger(1, "UDP receiver error: ", std::move(error));
}
