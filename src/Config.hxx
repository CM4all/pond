// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/SocketConfig.hxx"
#include "config.h"

#include <chrono>
#include <forward_list>

struct DatabaseConfig {
	size_t size = 16 * 1024 * 1024;

	/**
	 * A positive value means that records older than this
	 * duration will be deleted.
	 */
	std::chrono::system_clock::duration max_age{};

	double per_site_message_rate_limit = -1;
};

struct ListenerConfig : SocketConfig {
#ifdef HAVE_AVAHI
	std::string zeroconf_service;
#endif

	ListenerConfig() {
		listen = 64;
		tcp_defer_accept = 10;
		tcp_no_delay = true;
	}
};

struct Config {
	DatabaseConfig database;

	std::forward_list<SocketConfig> receivers;

	std::forward_list<ListenerConfig> listeners;

#ifdef HAVE_AVAHI
	bool auto_clone = false;
#endif // HAVE_AVAHI

	void Check();

#ifdef HAVE_AVAHI
	const ListenerConfig *GetZeroconfListener() const noexcept {
		for (const auto &i : listeners)
			if (!i.zeroconf_service.empty())
				return &i;

		return nullptr;
	}

	bool HasZeroconfListener() const noexcept {
		return GetZeroconfListener() != nullptr;
	}
#endif // HAVE_AVAHI
};

/**
 * Load and parse the specified configuration file.  Throws an
 * exception on error.
 */
void
LoadConfigFile(Config &config, const char *path);
