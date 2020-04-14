/*
 * Copyright 2017-2020 CM4all GmbH
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

#include "Instance.hxx"
#include "Config.hxx"
#include "AutoClone.hxx"
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

	if (config.auto_clone)
		instance.SetBlockingOperation(std::make_unique<AutoCloneOperation>(instance,
										   instance.GetDatabase(),
										   instance.GetAvahiClient(),
										   *config.GetZeroconfListener()));

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
