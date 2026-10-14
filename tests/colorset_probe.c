/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/Colorset.h"
#include "libs/Module.h"
#include <stdio.h>
#include <string.h>

/* A real module records exactly the IDs received in startup config packets.
 * No X connection or unbounded gap allocation is needed to load them. */
int main(int argc, char **argv)
{
	ModuleArgs *module = ParseModuleArgs(argc, argv, 0);
	if (module == NULL || module->user_argc != 1)
	{
		return 2;
	}
	int fd[] = {module->to_fvwm, module->from_fvwm};
	char *line;
	if (!AllocColorset(0))
	{
		return 3;
	}
	InitGetConfigLine(fd, NULL);
	GetConfigLine(fd, &line);
	while (line != NULL)
	{
		if (strncmp(line, "Colorset ", 8) == 0 &&
		    LoadColorset(line + 8) < 0)
		{
			return 4;
		}
		GetConfigLine(fd, &line);
	}
	FILE *report = fopen(module->user_argv[0], "w");
	if (report == NULL)
	{
		return 5;
	}
	fprintf(report, "{\"count\":%zu,\"ids\":[", ColorsetCount());
	for (size_t i = 0; i < ColorsetCount(); i++)
	{
		fprintf(report, "%s%d", i == 0 ? "" : ",", ColorsetIdAt(i));
	}
	fprintf(report, "]}\n");
	if (fclose(report) != 0)
	{
		return 6;
	}
	return 0;
}
