// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "CommandLine.hxx"
#include "Config.hxx"
#include "AutoClone.hxx"
#include "lib/avahi/Service.hxx"
#include "system/SetupProcess.hxx"
#include "system/ProcessName.hxx"
#include "util/PrintException.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <stdlib.h>

static void
Run(const Config &config)
{
	SetupProcess();

	Instance instance(config);

	if (config.auto_clone)
		instance.SetBlockingOperation(std::make_unique<AutoCloneOperation>(instance,
										   instance.GetDatabase(),
										   instance.GetAvahiClient(),
										   *config.GetZeroconfListener()));

	for (const auto &i : config.receivers)
		instance.AddReceiver(i);

	for (const auto &i : config.listeners)
		instance.AddListener(i);

	if (!config.auto_clone)
		instance.EnableZeroconf();

#ifdef HAVE_LIBSYSTEMD
	/* tell systemd we're ready */
	sd_notify(0, "READY=1");
#endif

	/* main loop */
	instance.Run();
}

int
main(int argc, char **argv) noexcept
try {
	const auto cmdline = ParseCommandLine(argc, argv);

	Config config;
	LoadConfigFile(config, cmdline.config_path);
	config.Check();

	Run(config);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
