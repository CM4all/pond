// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SimpleAddressResolver.hxx"
#include "util/PrintException.hxx"
#include "util/ScopeExit.hxx"

#include <avahi-client/lookup.h>

struct SimpleAddressResolver::Data {
	EventLoop &event_loop;

	std::string name;

	void Callback(AvahiResolverEvent event,
		      const char *_name) noexcept {
		if (event == AVAHI_RESOLVER_FOUND)
			name = _name;
		event_loop.Break();
	}

	static void Callback([[maybe_unused]] AvahiAddressResolver *r,
			     [[maybe_unused]] AvahiIfIndex interface,
			     [[maybe_unused]] AvahiProtocol protocol,
			     AvahiResolverEvent event,
			     [[maybe_unused]] const AvahiAddress *a,
			     const char *name,
			     [[maybe_unused]] AvahiLookupResultFlags flags,
			     void *userdata) noexcept {
		auto &data = *(Data *)userdata;
		data.Callback(event, name);
	}
};

std::string
SimpleAddressResolver::ResolveAddress(const char *address_string) noexcept
{
	AvahiAddress address_buffer;

	if (state == State::ERROR)
		return {};

	const auto *const address = avahi_address_parse(address_string,
							AVAHI_PROTO_UNSPEC,
							&address_buffer);
	if (address == nullptr)
		return {};

	if (state != State::READY) {
		/* wait until the Avahi client is connected */
		event_loop.Run();
		if (state != State::READY)
			return {};
	}

	Data data{event_loop};
	auto *const resolver =
		avahi_address_resolver_new(avahi_client.GetClient(),
					   AVAHI_IF_UNSPEC,
					   AVAHI_PROTO_UNSPEC,
					   //address->proto,
					   address,
					   AvahiLookupFlags{},
					   Data::Callback, &data);
	AtScopeExit(resolver) { avahi_address_resolver_free(resolver); };

	/* wait until the Avahi address resolver finishes */
	event_loop.Run();

	return std::move(data.name);
}

void
SimpleAddressResolver::OnAvahiConnect([[maybe_unused]] AvahiClient *client) noexcept
{
	state = State::READY;
	event_loop.Break();
}

void
SimpleAddressResolver::OnAvahiDisconnect() noexcept
{
	state = State::ERROR;
	event_loop.Break();
}

bool
SimpleAddressResolver::OnAvahiError(std::exception_ptr e) noexcept
{
	state = State::ERROR;
	PrintException(std::move(e));
	event_loop.Break();
	return true;
}
