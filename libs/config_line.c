/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */

#include "config.h"
#include "config_line.h"
#include "safemalloc.h"

int fvwm_config_line(FILE *stream, char **line, size_t *capacity)
{
	size_t used = 0;
	int byte;

	while ((byte = fgetc(stream)) != EOF)
	{
		if (byte == 0)
			return -1;
		if (byte == '\n')
		{
			if (used > 0 && (*line)[used - 1] == '\\')
			{
				used--;
				continue;
			}
			break;
		}
		if (used >= FVWM_CONFIG_LINE_MAX_BYTES)
			return -1;
		if (used + 2 > *capacity)
		{
			size_t next = *capacity ? *capacity * 2 : 1024;
			if (next > FVWM_CONFIG_LINE_MAX_BYTES + 1)
				next = FVWM_CONFIG_LINE_MAX_BYTES + 1;
			*line = fxrealloc(*line, next, sizeof(**line));
			*capacity = next;
		}
		(*line)[used++] = (char)byte;
	}
	if (ferror(stream))
		return -1;
	if (byte == EOF && used == 0)
		return 0;
	if (*line == NULL)
	{
		*line = fxmalloc(1);
		*capacity = 1;
	}
	(*line)[used] = '\0';
	return 1;
}
