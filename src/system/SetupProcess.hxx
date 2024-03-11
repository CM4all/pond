// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

/**
 * Set up the current process by applying some common settings.
 *
 * - ignore SIGPIPE
 * - disable pthread cancellation
 */
void
SetupProcess() noexcept;
