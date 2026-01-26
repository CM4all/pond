// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PerSitePath.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/FileAt.hxx"
#include "io/FileWriter.hxx"
#include "io/MakeDirectory.hxx"
#include "io/Open.hxx"

#include <fcntl.h> // for AT_FDCWD
#include <sys/stat.h>

PerSitePath::PerSitePath(const char *path, const char *_filename,
			 bool _nested)
	:directory(path != nullptr
		   ? OpenDirectoryPath({FileDescriptor{AT_FDCWD}, path})
		   : UniqueFileDescriptor{}),
	 filename(_filename),
	 nested(_nested)
{
}

class NestedSiteName {
	char buffer[64];
	char tail[3];

public:
	bool Set(const char *name) noexcept;

	const char *GetParent() const noexcept {
		return buffer;
	}

	const char *GetTail() const noexcept {
		return tail;
	}
};

inline bool
NestedSiteName::Set(const char *name) noexcept
{
	size_t length = strlen(name);
	if (length < 7 || length >= sizeof(buffer))
		return false;

	char *p = buffer;
	p = (char *)mempcpy(p, name, length - 6);
	*p++ = '/';
	*p++ = name[length - 6];
	*p++ = name[length - 5];
	*p++ = '/';
	*p++ = name[length - 4];
	*p++ = name[length - 3];
	*p = 0;

	tail[0] = name[length - 2];
	tail[1] = name[length - 1];
	tail[2] = 0;

	return true;
}

FileWriter
PerSitePath::Open(const char *site)
{
	FileDescriptor current_directory = directory;
	const char *current_filename = site;
	NestedSiteName nested_buffer;

	last_directory.Close();

	if (nested && nested_buffer.Set(site)) {
		last_directory = MakeNestedDirectory({current_directory, nested_buffer.GetParent()});
		current_directory = last_directory;
		current_filename = nested_buffer.GetTail();
	}

	if (filename != nullptr) {
		last_directory = MakeDirectory({current_directory, current_filename});
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
			throw FmtRuntimeError("Exists, but is not a regular file: {}",
					      current_filename);
	} else if (errno != ENOENT)
		throw FmtErrno("Failed to check output file: {}",
			       current_filename);

	return FileWriter({current_directory, current_filename});
}
