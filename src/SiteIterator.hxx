// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/SharedLease.hxx"

/**
 * This class is used by Database::GetFirstSite() and
 * Database::GetNextSite() to iterate over all sites.
 */
class SiteIterator {
	friend class Database;

	SharedLease lease;

	SiteIterator(SharedAnchor &anchor) noexcept
		:lease(anchor) {}

public:
	SiteIterator() noexcept = default;

	// these declarations prevent accidental copies
	SiteIterator(SiteIterator &&) = default;
	SiteIterator &operator=(SiteIterator &&) = default;

	constexpr operator bool() const noexcept {
		return static_cast<bool>(lease);
	}
};
