/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "script_command.h"
#include "libs/safemalloc.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool script_text_append(char **text, size_t *length, const char *value,
	size_t limit)
{
	size_t added = strlen(value);

	if (*length > limit || *length == SIZE_MAX ||
	    added > limit - *length || added > SIZE_MAX - 1 - *length)
		return false;
	*text = fxrealloc(*text, *length + added + 1, sizeof(char));
	memcpy(*text + *length, value, added + 1);
	*length += added;
	return true;
}

char *script_launch_command(const char *module, const char *argument,
	const char *selection)
{
	const char *name = argument + strspn(argument, " \t\r\n");
	size_t name_length = strcspn(name, " \t\r\n");
	char *filename, *command;

	if (name_length == 0 || strlen(argument) > SCRIPT_COMMAND_MAX_BYTES ||
	    strlen(module) > SCRIPT_COMMAND_MAX_BYTES ||
	    strlen(selection) > SCRIPT_COMMAND_MAX_BYTES)
		return NULL;
	filename = fxmalloc(name_length + 1);
	memcpy(filename, name, name_length);
	filename[name_length] = '\0';
	xasprintf(&command, "%s %s %s %s", module, filename, selection,
		name + name_length);
	free(filename);
	if (strlen(command) > SCRIPT_COMMAND_MAX_BYTES)
	{
		free(command);
		return NULL;
	}
	return command;
}
