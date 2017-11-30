/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Protocol.hxx"
#include "util/ConstBuffer.hxx"

#include <memory>

struct PondDatagram {
	uint16_t id;
	PondResponseCommand command;

	struct {
		std::unique_ptr<uint8_t[]> data;
		size_t size;

		operator ConstBuffer<void>() const noexcept {
			return {data.get(), size};
		}

		std::string ToString() const {
			return std::string((const char *)data.get(), size);
		}
	} payload;
};
