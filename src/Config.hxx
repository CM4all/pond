/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/SocketConfig.hxx"

#include <forward_list>

struct Config {
	std::forward_list<SocketConfig> receivers;

	void Check();
};

/**
 * Load and parse the specified configuration file.  Throws an
 * exception on error.
 */
void
LoadConfigFile(Config &config, const char *path);
