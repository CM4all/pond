// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string>

class UniqueSocketDescriptor;

struct PondServerSpecification {
	const char *host = nullptr;

	std::string zeroconf_service;
};

UniqueSocketDescriptor
PondConnect(const PondServerSpecification &spec);
