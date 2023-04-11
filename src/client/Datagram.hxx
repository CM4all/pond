// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Protocol.hxx"

#include <memory>
#include <span>
#include <string>

struct PondDatagram {
	uint16_t id;
	PondResponseCommand command;

	struct {
		std::unique_ptr<std::byte[]> data;
		size_t size;

		operator std::span<const std::byte>() const noexcept {
			return {data.get(), size};
		}

		std::string ToString() const {
			return std::string((const char *)data.get(), size);
		}
	} payload;
};
