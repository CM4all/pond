/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Config.hxx"
#include "system/SetupProcess.hxx"
#include "system/ProcessName.hxx"
#include "util/PrintException.hxx"

#include <systemd/sd-daemon.h>

#include <stdlib.h>

static void
Run(const Config &config)
{
	SetupProcess();

	Instance instance(config);

	for (const auto &i : config.receivers)
		instance.AddReceiver(i);

	for (const auto &i : config.listeners)
		instance.AddListener(i);

	/* tell systemd we're ready */
	sd_notify(0, "READY=1");

	/* main loop */
	instance.Dispatch();
}

int
main(int argc, char **)
try {
	if (argc != 1)
		throw std::runtime_error("Command line arguments not supported");

	Config config;
	LoadConfigFile(config, "/etc/cm4all/pond/pond.conf");
	config.Check();

	Run(config);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
