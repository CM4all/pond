// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class BlockingOperationHandler {
public:
	virtual void OnOperationFinished() noexcept = 0;
};

class BlockingOperation {
public:
	virtual ~BlockingOperation() noexcept = default;
};
