/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "filemanager") == 0)
	{
		static const char unavailable[] =
			"{\"version\":1,\"kind\":\"unavailable\",\"detail\":"
			"\"optional_bus_support_disabled\"}\n";
		int flags = fcntl(STDOUT_FILENO, F_GETFL);
		struct sigaction action = {0};

		action.sa_handler = SIG_IGN;
		if (flags >= 0 && !sigemptyset(&action.sa_mask) &&
		    !sigaction(SIGPIPE, &action, NULL) &&
		    !fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK))
			(void)write(STDOUT_FILENO, unavailable,
				sizeof(unavailable) - 1);
		return 1;
	}
	/* Refuse before any launch, including malformed guardian requests. */
	if (argc > 1 && strcmp(argv[1], "bus-guardian") == 0)
		return 2;
	if (argc > 1 && strncmp(argv[1], "power-", 6) == 0)
		return puts("{\"version\":1,\"kind\":\"power\",\"available\":false,"
			"\"state\":\"unavailable\",\"bus_id\":\"\",\"server_owner\":\"\","
			"\"detail\":\"optional_bus_support_disabled\",\"capabilities\":["
			"{\"action\":\"poweroff\",\"permission\":\"unknown\","
			"\"detail\":\"optional_bus_support_disabled\"},"
			"{\"action\":\"reboot\",\"permission\":\"unknown\","
			"\"detail\":\"optional_bus_support_disabled\"},"
			"{\"action\":\"suspend\",\"permission\":\"unknown\","
			"\"detail\":\"optional_bus_support_disabled\"}]}") < 0;
	return puts("{\"version\":1,\"available\":false,\"state\":\"unavailable\","
		"\"bus_id\":\"\",\"server_owner\":\"\",\"capabilities\":[],"
		"\"notification_id\":0,\"detail\":\"optional_bus_support_disabled\"}")
		< 0;
}
