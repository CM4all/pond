// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

/**
 * This class is used by Database::GetFirstSite() and
 * Database::GetNextSite() to iterate over all sites.
 */
class SiteIterator {
protected:
	SiteIterator() noexcept = default;
	~SiteIterator() noexcept = default;
	SiteIterator(const SiteIterator &) = delete;
	SiteIterator &operator=(const SiteIterator &) = delete;
};
