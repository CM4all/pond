/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/SocketConfig.hxx"

#include <forward_list>

struct DatabaseConfig {
	size_t size = 16 * 1024 * 1024;
};

struct Config {
	DatabaseConfig database;

	std::forward_list<SocketConfig> receivers;

	std::forward_list<SocketConfig> listeners;

	void Check();
};

/**
 * Load and parse the specified configuration file.  Throws an
 * exception on error.
 */
void
LoadConfigFile(Config &config, const char *path);
