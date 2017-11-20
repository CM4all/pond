/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Cursor.hxx"
#include "net/log/Datagram.hxx"
#include "util/ConstBuffer.hxx"

#include <boost/intrusive/list_hook.hpp>

#include <memory>

class Record final {
public:
	typedef boost::intrusive::list_member_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> ListHook;
	ListHook list_hook, per_site_list_hook;

private:
	uint64_t id;

	std::unique_ptr<uint8_t[]> raw;
	size_t raw_size;

	Net::Log::Datagram parsed;

	/**
	 * A list of cursors pointing to this record.  This is
	 * necessary to update pointers when removing this record.
	 */
	mutable CursorList cursors;

public:
	/**
	 * Throws Net::Log::ProtocolError on error.
	 */
	Record(uint64_t _id, ConstBuffer<uint8_t> _raw);

	Record(const Record &) = delete;
	Record &operator=(const Record &) = delete;

	uint64_t GetId() const {
		return id;
	}

	ConstBuffer<void> GetRaw() const {
		return {raw.get(), raw_size};
	}

	const Net::Log::Datagram &GetParsed() const {
		return parsed;
	}

	void AddCursor(Cursor &cursor) const {
		cursors.push_back(cursor);
	}

	void DisplaceCursors() {
		while (!cursors.empty())
			++cursors.front();
	}
};
