/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/Module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void marker(const char *prefix, const char *id, const char *suffix)
{
	char path[1024];
	int length = snprintf(
		path, sizeof(path), "%s.%s.%s", prefix, id, suffix);
	FILE *file;

	if (length < 0 || (size_t)length >= sizeof(path))
	{
		exit(2);
	}
	file = fopen(path, "w");
	if (file == NULL || fclose(file) != 0)
	{
		exit(3);
	}
}

int main(int argc, char **argv)
{
	int descriptors[2];
	char command[128];
	struct timespec delay = { 0, 10000000 };
	Bool synchronous;

	if (argc != 8)
	{
		return 2;
	}
	descriptors[0] = atoi(argv[1]);
	descriptors[1] = atoi(argv[2]);
	synchronous = strcmp(argv[7], "sync") == 0;
	SetMessageMask(descriptors, M_STRING);
	if (synchronous)
	{
		SetSyncMask(descriptors, M_STRING);
		SendText(descriptors, "Echo queue-sync-started", None);
		SendFinishedStartupNotification(descriptors);
	}
	else
	{
		marker(argv[6], argv[7], "ready");
		for (unsigned int i = 0; access(argv[6], F_OK) != 0; i++)
		{
			if (i >= 1000)
			{
				return 4;
			}
			nanosleep(&delay, NULL);
		}
		for (unsigned int i = 1; i <= 512; i++)
		{
			if (i % 16 == 0)
			{
				snprintf(
					command, sizeof(command), "Echo queue-%s-%u",
					argv[7], i / 16);
				SendText(descriptors, command, None);
			}
			else
			{
				SendText(descriptors, "Nop", None);
			}
			if (i == 32)
			{
				marker(argv[6], argv[7], "buffered");
			}
		}
	}
	/* Stay alive until the core has executed the burst. Closing either pipe
 * early could make output failure retire a producer with unread input. */
	for (;;)
	{
		FvwmPacket *packet = ReadFvwmPacket(descriptors[1]);
		if (packet == NULL)
		{
			return 5;
		}
		if (packet->type == M_STRING)
		{
			if (synchronous)
			{
				SendUnlockNotification(descriptors);
			}
			marker(argv[6], argv[7], "stopped");
			return 0;
		}
	}
}
