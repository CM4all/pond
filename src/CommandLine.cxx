// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CommandLine.hxx"
#include "util/StringAPI.hxx"

CommandLine
ParseCommandLine(int argc, char **argv)
{
	CommandLine cmdline;

	if (argc == 3 && StringIsEqual(argv[1], "--config"))
		cmdline.config_path = argv[2];
	else if (argc != 1)
		throw "Usage: cm4all-pond [--config PATH]";

	return cmdline;
}
