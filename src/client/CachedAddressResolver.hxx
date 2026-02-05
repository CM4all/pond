// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "SimpleAddressResolver.hxx"
#include "util/TransparentHash.hxx"

#include <unordered_map>

/**
 * A caching wrapper for #SimpleAddressResolver.
 */
class CachedAddressResolver final {
	SimpleAddressResolver simple;

	std::unordered_map<std::string, std::string,
			   TransparentHash, std::equal_to<>> cache;

public:
	[[gnu::pure]]
	const char *ResolveAddress(std::string address) noexcept;
};
