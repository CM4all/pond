/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/UniqueSocketDescriptor.hxx"
#include "event/net/BufferedSocket.hxx"

#include <boost/intrusive/list_hook.hpp>

#include <stdint.h>

enum class PondRequestCommand : uint16_t;
class Instance;
class RootLogger;
template<typename t> struct ConstBuffer;

class Connection final
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>,
	  BufferedSocketHandler {

	Instance &instance;
	const RootLogger &logger;

	BufferedSocket socket;

public:
	Connection(Instance &_instance, UniqueSocketDescriptor &&fd);
	~Connection();

	void Destroy() {
		delete this;
	}

private:
	BufferedResult OnPacket(uint16_t id, PondRequestCommand cmd,
				ConstBuffer<void> payload);

	/* virtual methods from class BufferedSocketHandler */
	BufferedResult OnBufferedData(const void *buffer, size_t size) override;
	bool OnBufferedClosed() noexcept override;
	bool OnBufferedWrite() override;
	void OnBufferedError(std::exception_ptr e) noexcept override;
};
