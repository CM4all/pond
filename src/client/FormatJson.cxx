// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "FormatJson.hxx"
#include "JsonWriter.hxx"
#include "http/Method.hxx"
#include "http/Status.hxx"
#include "time/Cast.hxx"
#include "time/ISO8601.hxx"
#include "net/log/String.hxx"
#include "net/log/Datagram.hxx"
#include "net/log/ContentType.hxx"
#include "util/StringBuffer.hxx"

char *
FormatJson(char *buffer, char *end,
	   const Net::Log::Datagram &d) noexcept
{
	JsonWriter::Object o{JsonWriter::Sink{buffer, end}};

	if (d.HasTimestamp()) {
		try {
			o.AddMember("time", FormatISO8601(d.timestamp).c_str());
		} catch (...) {
			/* just in case GmTime() throws */
		}
	}

	if (d.remote_host != nullptr)
		o.AddMember("remote_host", d.remote_host);

	if (d.host != nullptr)
		o.AddMember("host", d.host);

	if (d.site != nullptr)
		o.AddMember("site", d.site);

	if (d.analytics_id != nullptr)
		o.AddMember("analytics_id", d.analytics_id);

	if (d.generator != nullptr)
		o.AddMember("generator", d.generator);

	if (d.forwarded_to != nullptr)
		o.AddMember("forwarded_to", d.forwarded_to);

	if (d.HasHttpMethod() &&
	    http_method_is_valid(d.http_method))
		o.AddMember("method", http_method_to_string(d.http_method));

	if (d.http_uri.data() != nullptr)
		o.AddMember("uri", d.http_uri);

	if (d.http_referer.data() != nullptr)
		o.AddMember("referer", d.http_referer);

	if (d.user_agent.data() != nullptr)
		o.AddMember("user_agent", d.user_agent);

	if (d.message.data() != nullptr)
		o.AddMember("message", d.message);

	if (d.HasHttpStatus())
		o.AddMember("status", http_status_to_string(d.http_status));

	if (d.valid_length)
		o.AddMember("length", d.length);

	if (const auto content_type = Net::Log::ToString(d.content_type);
	    !content_type.empty())
		o.AddMember("content_type", content_type);

	if (d.valid_traffic) {
		o.AddMember("traffic_received", d.traffic_received);
		o.AddMember("traffic_sent", d.traffic_sent);
	}

	if (d.valid_duration)
		o.AddMember("duration", ToFloatSeconds(d.duration));

	if (d.type != Net::Log::Type::UNSPECIFIED) {
		const char *type = ToString(d.type);
		if (type != nullptr)
			o.AddMember("type", type);
	}

	o.Flush();

	return o.GetPosition();
}
