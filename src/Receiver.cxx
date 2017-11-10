/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "event/net/UdpListener.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketConfig.hxx"
#include "net/log/Parser.hxx"
#include "util/PrintException.hxx"

bool
Instance::OnUdpDatagram(const void *data, size_t length,
			SocketAddress address, int uid)
{
	(void)address;
	(void)uid;

	try {
		database.Emplace({(const uint8_t *)data, length});
	} catch (Net::Log::ProtocolError) {
	}

	return true;
}

void
Instance::OnUdpError(std::exception_ptr ep) noexcept
{
	logger(1, "UDP receiver error: ", ep);
}
