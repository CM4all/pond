// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/log/Chrono.hxx"
#include "net/log/Protocol.hxx"

#include <cstddef>
#include <cstdint>
#include <string>
#include <span>
#include <set>

struct SmallDatagram;
namespace Net { namespace Log { struct Datagram; }}

struct Filter {
	std::set<std::string, std::less<>> sites, hosts, generators;

	std::string http_uri_starts_with;

	Net::Log::TimePoint since = Net::Log::TimePoint::min();
	Net::Log::TimePoint until = Net::Log::TimePoint::max();

	Net::Log::Type type = Net::Log::Type::UNSPECIFIED;

	struct {
		uint16_t begin{}, end{UINT16_MAX};

		constexpr bool operator()(uint16_t status) const noexcept {
			return status >= begin && status < end;
		}

		operator bool() const noexcept {
			return begin != 0 || end != UINT16_MAX;
		}
	} http_status;

	bool HasOneSite() const noexcept {
		return !sites.empty() &&
			std::next(sites.begin()) == sites.end();
	}

	[[gnu::pure]]
	bool operator()(const SmallDatagram &d, std::span<const std::byte> raw) const noexcept;

	[[gnu::pure]]
	bool operator()(const Net::Log::Datagram &d) const noexcept;

private:
	[[gnu::pure]]
	bool MatchMore(std::span<const std::byte> raw) const noexcept;
};
