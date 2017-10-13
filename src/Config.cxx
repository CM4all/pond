/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Config.hxx"
#include "net/Parser.hxx"
#include "io/FileLineParser.hxx"
#include "io/ConfigParser.hxx"

void
Config::Check()
{
	if (receivers.empty())
		throw std::runtime_error("No 'receiver' configured");
}

class PondConfigParser final : public NestedConfigParser {
	Config &config;

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
		SocketConfig config;

	public:
		explicit Listener(Config &_parent):parent(_parent) {
			config.listen = 64;
			config.tcp_defer_accept = 10;
		}

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
PondConfigParser::Receiver::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (strcmp(word, "bind") == 0) {
		config.bind_address = ParseSocketAddress(line.ExpectValueAndEnd(),
							 5479, true);
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

	parent.receivers.emplace_front(std::move(config));

	ConfigParser::Finish();
}

void
PondConfigParser::Listener::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (strcmp(word, "bind") == 0) {
		config.bind_address = ParseSocketAddress(line.ExpectValueAndEnd(),
							 5480, true);
	} else if (strcmp(word, "interface") == 0) {
		config.interface = line.ExpectValueAndEnd();
	} else
		throw LineParser::Error("Unknown option");
}

void
PondConfigParser::Listener::Finish()
{
	if (config.bind_address.IsNull())
		throw LineParser::Error("Listener has no bind address");

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
