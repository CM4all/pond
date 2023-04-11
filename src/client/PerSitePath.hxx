// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"

class FileWriter;

class PerSitePath {
	/**
	 * Inside this directory, a file will be created for each
	 * site.
	 */
	const UniqueFileDescriptor directory;

	/**
	 * If set, then a new directory is created for each site, and
	 * this is the filename inside the directory.
	 *
	 * The pointed-to string is owned by #QueryOptions.
	 */
	const char *const filename;

	const bool nested;

	/**
	 * This attribute is a kludge to keep the directory file
	 * handle open for use by FileWriter::Commit().
	 */
	UniqueFileDescriptor last_directory;

public:
	PerSitePath(const char *path, const char *_filename,
		    bool _nested);

	PerSitePath(const PerSitePath &) = delete;
	PerSitePath &operator=(const PerSitePath &) = delete;

	bool IsDefined() const noexcept {
		return directory.IsDefined();
	}

	/**
	 * May return an undefined object if this site shall be
	 * skipped.
	 */
	FileWriter Open(const char *site);
};
