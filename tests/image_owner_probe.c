/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/Module.h"

#include <stdio.h>
#include <string.h>

/* Observe real mini-icon packets without querying X or inspecting core
 * memory. The harness owns the private report and this module's lifetime. */
int main(int argc, char **argv)
{
	ModuleArgs *args = ParseModuleArgs(argc, argv, 0);

	if (args == NULL || (args->user_argc != 1 &&
	    (args->user_argc != 2 ||
	     strcmp(args->user_argv[0], "colorsets") != 0)))
	{
		return 2;
	}
	Bool colorsets = args->user_argc == 2;
	FILE *report = fopen(args->user_argv[colorsets ? 1 : 0], "w");

	if (report == NULL)
	{
		return 3;
	}
	int descriptors[2] = {args->to_fvwm, args->from_fvwm};

	SetMessageMask(descriptors, colorsets ? M_CONFIG_INFO : M_MINI_ICON);
	SendText(descriptors, colorsets ? "Send_ConfigInfo" : "Send_WindowList",
	    None);
	for (;;)
	{
		FvwmPacket *packet = ReadFvwmPacket(descriptors[1]);

		if (packet == NULL)
		{
			return fclose(report) == 0 ? 0 : 4;
		}
		if (packet->type == M_MINI_ICON)
		{
			if (FvwmPacketBodySize(*packet) < 9 ||
			    fprintf(report, "%lu %lu %lu %lu\n",
			    packet->body[0], packet->body[6],
			    packet->body[3], packet->body[4]) < 0 ||
			    fflush(report) != 0)
			{
				fclose(report);
				return 5;
			}
		}
		else if (colorsets && packet->type == M_CONFIG_INFO &&
		    FvwmPacketBodySize(*packet) > 3)
		{
			const char *text = (const char *)&packet->body[3];
			size_t bytes = (FvwmPacketBodySize(*packet) - 3) *
			    sizeof(unsigned long);

			if (memchr(text, '\0', bytes) == NULL)
			{
				fclose(report);
				return 6;
			}
			if (strncmp(text, "Colorset ", 8) == 0 &&
			    (fprintf(report, "%s\n", text) < 0 ||
			     fflush(report) != 0))
			{
				fclose(report);
				return 7;
			}
		}
	}
}
