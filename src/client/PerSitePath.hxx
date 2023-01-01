/*
 * Copyright 2017-2022 CM4all GmbH
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
