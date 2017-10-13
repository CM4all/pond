/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/UniqueSocketDescriptor.hxx"
#include "event/net/BufferedSocket.hxx"

#include <boost/intrusive/list_hook.hpp>

class Instance;
class RootLogger;

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
	/* virtual methods from class BufferedSocketHandler */
	BufferedResult OnBufferedData(const void *buffer, size_t size) override;
	bool OnBufferedClosed() override;
	bool OnBufferedWrite() override;
	void OnBufferedError(std::exception_ptr e) override;
};
