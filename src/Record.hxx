/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/log/Datagram.hxx"
#include "util/ConstBuffer.hxx"

#include <boost/intrusive/list_hook.hpp>

class Record final {
public:
	typedef boost::intrusive::list_member_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>> ListHook;
	ListHook per_site_list_hook;

private:
	uint64_t id;

	const size_t raw_size;

	Net::Log::Datagram parsed;

public:
	/**
	 * Throws Net::Log::ProtocolError on error.
	 */
	Record(uint64_t _id, ConstBuffer<uint8_t> _raw);

	Record(const Record &) = delete;
	Record &operator=(const Record &) = delete;

	uint64_t GetId() const noexcept {
		return id;
	}

	ConstBuffer<void> GetRaw() const noexcept {
		return {this + 1, raw_size};
	}

	const Net::Log::Datagram &GetParsed() const noexcept {
		return parsed;
	}
};
