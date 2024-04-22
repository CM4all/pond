// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>

namespace Net::Log { struct Datagram; }

char *
FormatJson(char *dest, char *end,
	   const Net::Log::Datagram &d) noexcept;
