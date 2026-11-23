// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

class BlockingOperationHandler {
public:
	virtual void OnOperationFinished() noexcept = 0;
};

class BlockingOperation {
public:
	virtual ~BlockingOperation() noexcept = default;
};
