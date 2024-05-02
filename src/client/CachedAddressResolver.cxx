// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CachedAddressResolver.hxx"

const char *
CachedAddressResolver::ResolveAddress(std::string address) noexcept
{
	auto i = cache.find(address);
	if (i == cache.end()) {
		auto name = simple.ResolveAddress(address.c_str());
		i = cache.emplace(std::move(address), std::move(name)).first;
	}

	return i->second.empty() ? nullptr : i->second.c_str();
}
