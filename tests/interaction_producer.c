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

static void path_name(char *path, size_t size, const char *prefix,
	const char *mode, const char *suffix)
{
	int length = snprintf(path, size, "%s.%s.%s", prefix, mode, suffix);
	if (length < 0 || (size_t)length >= size)
	{
		exit(2);
	}
}

static void marker(const char *prefix, const char *mode, const char *suffix)
{
	char path[1024];
	FILE *file;

	path_name(path, sizeof(path), prefix, mode, suffix);
	file = fopen(path, "w");
	if (file == NULL || fclose(file) != 0)
	{
		exit(3);
	}
}

int main(int argc, char **argv)
{
	int descriptors[2];
	char path[1024], command[1024];
	struct timespec delay = { 0, 10000000 };
	const char *prefix, *mode;
	Bool synchronous;

	if (argc != 8)
	{
		return 2;
	}
	descriptors[0] = atoi(argv[1]);
	descriptors[1] = atoi(argv[2]);
	prefix = argv[6];
	mode = argv[7];
	synchronous = strcmp(mode, "sync") == 0;
	SetMessageMask(descriptors, M_STRING);
	if (synchronous)
	{
		SetSyncMask(descriptors, M_STRING);
	}
	snprintf(command, sizeof(command), "Echo service-producer-%s-ready", mode);
	SendText(descriptors, command, None);
	marker(prefix, mode, "ready");
	if (strcmp(mode, "async") != 0 && !synchronous)
	{
		FILE *file;
		path_name(path, sizeof(path), prefix, mode, "gate");
		for (unsigned int i = 0; access(path, F_OK) != 0; i++)
		{
			if (i >= 2000)
			{
				return 4;
			}
			nanosleep(&delay, NULL);
		}
		path_name(path, sizeof(path), prefix, mode, "commands");
		file = fopen(path, "r");
		if (file == NULL)
		{
			return 5;
		}
		while (fgets(command, sizeof(command), file) != NULL)
		{
			command[strcspn(command, "\n")] = '\0';
			SendText(descriptors, command, None);
		}
		if (ferror(file) || fclose(file) != 0)
		{
			return 6;
		}
		marker(prefix, mode, "queued");
	}
	for (;;)
	{
		FvwmPacket *packet = ReadFvwmPacket(descriptors[1]);
		if (packet == NULL)
		{
			return 0;
		}
		if (packet->type == M_STRING)
		{
			if (synchronous)
			{
				/* Detect unsafe synchronous reentry: Style updates must only
				 * be reached after the enclosing drag has returned. */
				SendText(descriptors, "Style * BorderWidth 9, HandleWidth 9", None);
				SendText(descriptors, "UpdateStyles", None);
				SendUnlockNotification(descriptors);
			}
			snprintf(command, sizeof(command), "Echo service-%s-reply", mode);
			SendText(descriptors, command, None);
		}
	}
}
