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
	const char *suffix)
{
	int length = snprintf(path, size, "%s.%s", prefix, suffix);
	if (length < 0 || (size_t)length >= size)
	{
		exit(2);
	}
}

int main(int argc, char **argv)
{
	int descriptors[2];
	char path[1024], armed[1024], unlock[1024];
	struct timespec delay = { 0, 10000000 };
	const char *prefix, *mode;

	if (argc != 8)
	{
		return 2;
	}
	descriptors[0] = atoi(argv[1]);
	descriptors[1] = atoi(argv[2]);
	prefix = argv[6];
	mode = argv[7];
	path_name(armed, sizeof(armed), prefix, "armed");
	path_name(unlock, sizeof(unlock), prefix, "unlock");
	SetMessageMask(descriptors, M_CONFIGURE_WINDOW | M_DESTROY_WINDOW);
	if (strcmp(mode, "async") != 0)
	{
		SetSyncMask(descriptors, M_CONFIGURE_WINDOW);
	}
	SendText(descriptors, "Echo frame-sync-ready", None);
	for (;;)
	{
		FvwmPacket *packet = ReadFvwmPacket(descriptors[1]);
		FILE *file;
		if (packet == NULL)
		{
			return 0;
		}
		if (packet->type != M_CONFIGURE_WINDOW &&
		    packet->type != M_DESTROY_WINDOW)
		{
			continue;
		}
		if (access(armed, F_OK) == 0)
		{
			path_name(path, sizeof(path), prefix, "packets");
			file = fopen(path, "a");
			if (file == NULL)
			{
				return 3;
			}
			if (packet->type == M_CONFIGURE_WINDOW)
			{
				fprintf(file, "config %lu %ld %ld %lu %lu\n",
					packet->body[0], (long)packet->body[3],
					(long)packet->body[4], packet->body[5],
					packet->body[6]);
			}
			else
			{
				fprintf(file, "destroy %lu\n", packet->body[0]);
			}
			if (fclose(file) != 0)
			{
				return 3;
			}
			if (packet->type == M_CONFIGURE_WINDOW &&
			    strcmp(mode, "async") != 0)
			{
				if (strcmp(mode, "disconnect") == 0)
				{
					return 0;
				}
				for (unsigned int i = 0; access(unlock, F_OK) != 0; i++)
				{
					if (i >= 2000)
					{
						return 4;
					}
					nanosleep(&delay, NULL);
				}
				/* Exercise a real mutating reply after outer unwind. */
				SendText(descriptors, "Style * BorderWidth 7", None);
				SendText(descriptors, "UpdateStyles", None);
			}
		}
		if (packet->type == M_CONFIGURE_WINDOW &&
		    strcmp(mode, "async") != 0)
		{
			SendUnlockNotification(descriptors);
		}
	}
}
