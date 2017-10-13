/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/UdpListener.hxx"
#include "net/SocketConfig.hxx"
#include "net/log/Parser.hxx"
#include "util/PrintException.hxx"

void
Instance::OnUdpDatagram(const void *data, size_t length,
			SocketAddress address, int uid)
{
	(void)address;
	(void)uid;

	try {
		const auto d = database.Emplace({(const uint8_t *)data, length}).GetParsed();

		// TODO: insert into database
		if (d.http_uri != nullptr)
			logger(1, "received http_uri='", d.http_uri, "'");
		else if (d.message != nullptr)
			logger(1, "received message='", d.message, "'");
	} catch (Net::Log::ProtocolError) {
	}
}

void
Instance::OnUdpError(std::exception_ptr ep)
{
	logger(1, "UDP receiver error: ", ep);
}
