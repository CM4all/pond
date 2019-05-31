/*
 * Copyright 2017-2019 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Config.hxx"
#include "Port.hxx"
#include "avahi/Check.hxx"
#include "net/Parser.hxx"
#include "io/FileLineParser.hxx"
#include "io/ConfigParser.hxx"
#include "pg/Interval.hxx"
#include "util/StringParser.hxx"

void
Config::Check()
{
	if (receivers.empty())
		throw std::runtime_error("No 'receiver' configured");
}

class PondConfigParser final : public NestedConfigParser {
	Config &config;

	class Database final : public ConfigParser {
		DatabaseConfig &config;

	public:
		explicit Database(DatabaseConfig &_config):config(_config) {}

	protected:
		/* virtual methods from class ConfigParser */
		void ParseLine(FileLineParser &line) override;
	};

	class Receiver final : public ConfigParser {
		Config &parent;
		SocketConfig config;

	public:
		explicit Receiver(Config &_parent):parent(_parent) {}

	protected:
		/* virtual methods from class ConfigParser */
		void ParseLine(FileLineParser &line) override;
		void Finish() override;
	};

	class Listener final : public ConfigParser {
		Config &parent;
		ListenerConfig config;

	public:
		explicit Listener(Config &_parent):parent(_parent) {}

	protected:
		/* virtual methods from class ConfigParser */
		void ParseLine(FileLineParser &line) override;
		void Finish() override;
	};

public:
	explicit PondConfigParser(Config &_config)
		:config(_config) {}

protected:
	/* virtual methods from class NestedConfigParser */
	void ParseLine2(FileLineParser &line) override;
};

void
PondConfigParser::Database::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (strcmp(word, "size") == 0) {
		config.size = ParseSize(line.ExpectValueAndEnd());
		if (config.size < 64 * 1024)
			throw LineParser::Error("Database size is too small");
	} else if (strcmp(word, "max_age") == 0) {
		config.max_age = Pg::ParseIntervalS(line.ExpectValueAndEnd());
		if (config.max_age <= std::chrono::system_clock::duration::zero())
			throw LineParser::Error("max_age too small");
	} else if (strcmp(word, "per_site_message_rate_limit") == 0) {
		config.per_site_message_rate_limit = ParsePositiveLong(line.ExpectValueAndEnd());
	} else
		throw LineParser::Error("Unknown option");
}

void
PondConfigParser::Receiver::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (strcmp(word, "bind") == 0) {
		config.bind_address = ParseSocketAddress(line.ExpectValueAndEnd(),
							 5479, true);
	} else if (strcmp(word, "v6only") == 0) {
		const bool value = line.NextBool();
		line.ExpectEnd();

		if (!value)
			throw std::runtime_error("Explicitly disabling v6only is not implemented");

		config.v6only = value;
	} else if (strcmp(word, "multicast_group") == 0) {
		config.multicast_group = ParseSocketAddress(line.ExpectValueAndEnd(),
							    0, false);
	} else if (strcmp(word, "interface") == 0) {
		config.interface = line.ExpectValueAndEnd();
	} else
		throw LineParser::Error("Unknown option");
}

void
PondConfigParser::Receiver::Finish()
{
	if (config.bind_address.IsNull())
		throw LineParser::Error("Listener has no bind address");

	config.Fixup();

	parent.receivers.emplace_front(std::move(config));

	ConfigParser::Finish();
}

void
PondConfigParser::Listener::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (strcmp(word, "bind") == 0) {
		config.bind_address = ParseSocketAddress(line.ExpectValueAndEnd(),
							 POND_DEFAULT_PORT, true);
	} else if (strcmp(word, "interface") == 0) {
		config.interface = line.ExpectValueAndEnd();
	} else if (strcmp(word, "zeroconf_service") == 0) {
		config.zeroconf_service = MakeZeroconfServiceType(line.ExpectValueAndEnd(),
								  "_tcp");
	} else
		throw LineParser::Error("Unknown option");
}

void
PondConfigParser::Listener::Finish()
{
	if (config.bind_address.IsNull())
		throw LineParser::Error("Listener has no bind address");

	config.Fixup();

	parent.listeners.emplace_front(std::move(config));

	ConfigParser::Finish();
}

void
PondConfigParser::ParseLine2(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (strcmp(word, "receiver") == 0) {
		line.ExpectSymbolAndEol('{');
		SetChild(std::make_unique<Receiver>(config));
	} else if (strcmp(word, "listener") == 0) {
		line.ExpectSymbolAndEol('{');
		SetChild(std::make_unique<Listener>(config));
	} else if (strcmp(word, "database") == 0) {
		line.ExpectSymbolAndEol('{');
		SetChild(std::make_unique<Database>(config.database));
	} else
		throw LineParser::Error("Unknown option");
}

void
LoadConfigFile(Config &config, const char *path)
{
	PondConfigParser parser(config);
	VariableConfigParser v_parser(parser);
	CommentConfigParser parser2(v_parser);
	IncludeConfigParser parser3(path, parser2);

	ParseConfigFile(path, parser3);
}
