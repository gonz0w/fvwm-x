/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/config_line.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void check(const char *input, size_t size, const char *expected, int status)
{
	FILE *stream = tmpfile();
	char *line = NULL;
	size_t capacity = 0;

	assert(stream != NULL && fwrite(input, 1, size, stream) == size);
	rewind(stream);
	assert(fvwm_config_line(stream, &line, &capacity) == status);
	if (status == 1)
	{
		assert(strcmp(line, expected) == 0);
		assert(fvwm_config_line(stream, &line, &capacity) == 0);
	}
	free(line);
	fclose(stream);
}

int main(void)
{
	check("", 0, NULL, 0);
	check("\n", 1, "", 1);
	check("last\\", 5, "last\\", 1);
	check("first\\\n second\\\n third\n", 23, "first second third", 1);
	check("binary\0tail\n", 12, NULL, -1);
	char *long_line = malloc(FVWM_CONFIG_LINE_MAX_BYTES + 2);
	assert(long_line != NULL);
	memset(long_line, 'x', FVWM_CONFIG_LINE_MAX_BYTES + 1);
	long_line[FVWM_CONFIG_LINE_MAX_BYTES] = '\0';
	check(long_line, FVWM_CONFIG_LINE_MAX_BYTES, long_line, 1);
	long_line[FVWM_CONFIG_LINE_MAX_BYTES] = 'x';
	check(long_line, FVWM_CONFIG_LINE_MAX_BYTES + 1, NULL, -1);
	free(long_line);
	return 0;
}
