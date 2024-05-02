// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/Loop.hxx"
#include "lib/avahi/Client.hxx"
#include "lib/avahi/ConnectionListener.hxx"
#include "lib/avahi/ErrorHandler.hxx"

#include <string>

/**
 * A simple synchronous wrapper for the Avahi address resolver.  An
 * EventLoop is used to execute the Avahi client, and completion
 * invokes EventLoop::Break().  This is a kludge to make the client
 * synchronous.
 */
class SimpleAddressResolver final : Avahi::ConnectionListener, Avahi::ErrorHandler {
	EventLoop event_loop;
	Avahi::Client avahi_client{event_loop, *this};

	enum class State {
		INIT,
		READY,
		ERROR,
	} state = State::INIT;

	struct Data;

public:
	SimpleAddressResolver() noexcept {
		avahi_client.AddListener(*this);
	}

	~SimpleAddressResolver() noexcept {
		avahi_client.RemoveListener(*this);
	}

	[[gnu::pure]]
	std::string ResolveAddress(const char *address) noexcept;

private:
	// virtual methods from class Avahi::ConnectionListener
	void OnAvahiConnect(AvahiClient *client) noexcept override;
	void OnAvahiDisconnect() noexcept override;

	// virtual methods from class Avahi::ErrorHandler
	bool OnAvahiError(std::exception_ptr e) noexcept override;
};
