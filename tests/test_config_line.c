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
	size_t next = 1, first = 99, last = 99;

	assert(stream != NULL && fwrite(input, 1, size, stream) == size);
	rewind(stream);
	assert(fvwm_config_line_span(stream, &line, &capacity,
		&next, &first, &last) == status);
	if (status <= 0)
		assert(first == 0 && last == 0);
	if (status == 1)
	{
		assert(strcmp(line, expected) == 0);
		assert(fvwm_config_line(stream, &line, &capacity) == 0);
	}
	rewind(stream);
	assert(fvwm_config_line(stream, &line, &capacity) == status);
	if (status == 1)
		assert(strcmp(line, expected) == 0);
	free(line);
	fclose(stream);
}

static void spans(void)
{
	FILE *stream = tmpfile();
	char *line = NULL;
	size_t capacity = 0, next = 1, first = 99, last = 99;
	const char *input = "a\\\n b\n\nlast";
	assert(stream != NULL);
	assert(fwrite(input, 1, strlen(input), stream) == strlen(input));
	rewind(stream);
	assert(fvwm_config_line_span(stream, &line, &capacity,
		&next, &first, &last) == 1);
	assert(first == 1 && last == 2 && next == 3);
	assert(strcmp(line, "a b") == 0);
	assert(fvwm_config_line_span(stream, &line, &capacity,
		&next, &first, &last) == 1);
	assert(first == 3 && last == 3 && next == 4);
	assert(fvwm_config_line_span(stream, &line, &capacity,
		&next, &first, &last) == 1);
	assert(first == 4 && last == 4 && next == 5);
	assert(fvwm_config_line_span(stream, &line, &capacity,
		&next, &first, &last) == 0);
	assert(first == 0 && last == 0);
	free(line);
	fclose(stream);
}

int main(void)
{
	spans();
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
	/* Normalized length stays zero, but physical input remains bounded. */
	char *continued = malloc(FVWM_CONFIG_LINE_MAX_BYTES + 2);
	assert(continued != NULL);
	for (size_t i = 0; i < FVWM_CONFIG_LINE_MAX_BYTES + 2; i += 2)
	{
		continued[i] = '\\';
		continued[i + 1] = '\n';
	}
	check(continued, FVWM_CONFIG_LINE_MAX_BYTES + 2, NULL, -1);
	free(continued);
	return 0;
}
