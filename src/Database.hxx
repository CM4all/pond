/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/log/Datagram.hxx"
#include "util/ConstBuffer.hxx"

#include <memory>
#include <list>

class Record {
	std::unique_ptr<uint8_t[]> raw;

	Net::Log::Datagram parsed;

public:
	/**
	 * Throws Net::Log::ProtocolError on error.
	 */
	explicit Record(ConstBuffer<uint8_t> _raw);

	Record(const Record &) = delete;
	Record &operator=(const Record &) = delete;

	const Net::Log::Datagram &GetParsed() const {
		return parsed;
	}
};

class Database {
	std::list<Record> records;

public:
	const Record &Emplace(ConstBuffer<uint8_t> raw) {
		records.emplace_back(raw);
		return records.back();
	}
};
