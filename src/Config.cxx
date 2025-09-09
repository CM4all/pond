// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Config.hxx"
#include "Port.hxx"
#include "net/Parser.hxx"
#include "net/log/Protocol.hxx"
#include "io/config/FileLineParser.hxx"
#include "io/config/ConfigParser.hxx"
#include "pg/Interval.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "util/StringParser.hxx"

#ifdef HAVE_AVAHI
#include "lib/avahi/Check.hxx"
#endif

using std::string_view_literals::operator""sv;

void
Config::Check()
{
	if (receivers.empty())
		throw std::runtime_error("No 'receiver' configured");

#ifdef HAVE_AVAHI
	if (auto_clone && !HasZeroconfListener())
		throw LineParser::Error("'auto_clone' requires a Zeroconf listener");
#endif // HAVE_AVAHI
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

	if (StringIsEqual(word, "size")) {
		config.size = ParseSize(line.ExpectValueAndEnd());
		if (config.size < 64 * 1024)
			throw LineParser::Error("Database size is too small");
	} else if (StringIsEqual(word, "max_age")) {
		config.max_age = Pg::ParseIntervalS(line.ExpectValueAndEnd());
		if (config.max_age <= std::chrono::system_clock::duration::zero())
			throw LineParser::Error("max_age too small");
	} else if (StringIsEqual(word, "per_site_message_rate_limit")) {
		config.per_site_message_rate_limit = ParsePositiveLong(line.ExpectValueAndEnd());
	} else
		throw LineParser::Error("Unknown option");
}

void
PondConfigParser::Receiver::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (StringIsEqual(word, "bind")) {
		config.bind_address = ParseSocketAddress(line.ExpectValueAndEnd(),
							 Net::Log::DEFAULT_PORT, true);
	} else if (StringIsEqual(word, "v6only")) {
		const bool value = line.NextBool();
		line.ExpectEnd();

		if (!value)
			throw std::runtime_error("Explicitly disabling v6only is not implemented");

		config.v6only = value;
	} else if (StringIsEqual(word, "multicast_group")) {
		config.multicast_group = ParseSocketAddress(line.ExpectValueAndEnd(),
							    0, false);
	} else if (StringIsEqual(word, "interface")) {
		config.interface = line.ExpectValueAndEnd();
	} else if (StringIsEqual(word, "mptcp")) {
		config.mptcp = line.NextBool();
		line.ExpectEnd();
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

	if (StringIsEqual(word, "bind")) {
		config.bind_address = ParseSocketAddress(line.ExpectValueAndEnd(),
							 POND_DEFAULT_PORT, true);
	} else if (StringIsEqual(word, "interface")) {
		config.interface = line.ExpectValueAndEnd();
#ifdef HAVE_AVAHI
	} else if (config.zeroconf.ParseLine(word, line)) {
#else
	} else if (StringStartsWith(word, "zeroconf_"sv)) {
		throw std::runtime_error{"Zeroconf support is disabled"};
#endif // HAVE_AVAHI
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

	if (StringIsEqual(word, "receiver")) {
		line.ExpectSymbolAndEol('{');
		SetChild(std::make_unique<Receiver>(config));
	} else if (StringIsEqual(word, "listener")) {
		line.ExpectSymbolAndEol('{');
		SetChild(std::make_unique<Listener>(config));
	} else if (StringIsEqual(word, "database")) {
		line.ExpectSymbolAndEol('{');
		SetChild(std::make_unique<Database>(config.database));
	} else if (StringIsEqual(word, "auto_clone")) {
#ifdef HAVE_AVAHI
		const bool value = line.NextBool();
		line.ExpectEnd();

		config.auto_clone = value;
#else
		throw std::runtime_error{"Zeroconf support is disabled"};
#endif // HAVE_AVAHI
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
