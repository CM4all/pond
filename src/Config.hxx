/*
 * Copyright 2017-2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "net/SocketConfig.hxx"

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
	std::string zeroconf_service;

	ListenerConfig() {
		listen = 64;
		tcp_defer_accept = 10;
	}
};

struct Config {
	DatabaseConfig database;

	std::forward_list<SocketConfig> receivers;

	std::forward_list<ListenerConfig> listeners;

	bool auto_clone = false;

	void Check();

	const ListenerConfig *GetZeroconfListener() const noexcept {
		for (const auto &i : listeners)
			if (!i.zeroconf_service.empty())
				return &i;

		return nullptr;
	}

	bool HasZeroconfListener() const noexcept {
		return GetZeroconfListener() != nullptr;
	}
};

/**
 * Load and parse the specified configuration file.  Throws an
 * exception on error.
 */
void
LoadConfigFile(Config &config, const char *path);
