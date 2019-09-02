/*
 * Copyright 2017-2019 Content Management AG
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

#include "PerSitePath.hxx"
#include "io/FileWriter.hxx"
#include "io/MakeDirectory.hxx"
#include "io/Open.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"

#include <fcntl.h>
#include <sys/stat.h>

PerSitePath::PerSitePath(const char *path, const char *_filename) noexcept
	:directory(path != nullptr
		   ? OpenPath(path, O_DIRECTORY)
		   : UniqueFileDescriptor{}),
	 filename(_filename)
{
}

FileWriter
PerSitePath::Open(const char *site)
{
	FileDescriptor current_directory = directory;
	const char *current_filename = site;

	if (filename != nullptr) {
		last_directory.Close();
		last_directory = MakeDirectory(current_directory,
					       current_filename);
		current_directory = last_directory;
		current_filename = filename;
	}

	struct stat st;
	if (fstatat(current_directory.Get(), current_filename,
		    &st, AT_SYMLINK_NOFOLLOW) == 0) {
		if (S_ISREG(st.st_mode))
			/* exists already: skip */
			return {};
		else
			throw FormatRuntimeError("Exists, but is not a regular file: %s",
						 current_filename);
	} else if (errno != ENOENT)
		throw FormatErrno("Failed to check output file: %s",
				  current_filename);

	return FileWriter(current_directory, current_filename);
}
