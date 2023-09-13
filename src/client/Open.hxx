// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "config.h"

#include <string>

class UniqueSocketDescriptor;

struct PondServerSpecification {
	const char *host = nullptr;

#ifdef HAVE_AVAHI
	std::string zeroconf_service;
#endif
};

UniqueSocketDescriptor
PondConnect(const PondServerSpecification &spec);
