// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct CommandLine {
	const char *config_path = "/etc/cm4all/pond/pond.conf";
};

CommandLine
ParseCommandLine(int argc, char **argv);
