/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "module_monitor.h"
#include "Module.h"
#include "Parse.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

bool module_parse_decimal_integers(const char *text, int *values, size_t count)
{
	if (text == NULL || values == NULL)
	{
		return false;
	}
	for (size_t i = 0; i < count; i++)
	{
		char *end;
		long value;

		errno = 0;
		value = strtol(text, &end, 10);
		if (end == text || errno == ERANGE ||
			value < INT_MIN || value > INT_MAX)
		{
			return false;
		}
		values[i] = (int)value;
		text = end;
	}
	return true;
}

bool module_monitor_format(char *output, size_t capacity, const char *name,
	const int values[MODULE_MONITOR_VALUES])
{
	char token[FvwmPacketBodyMaxSize * sizeof(unsigned long)];
	size_t used = 0;
	bool quoted = false;

	if (output == NULL || capacity == 0)
	{
		return false;
	}
	output[0] = '\0';
	if (name == NULL || values == NULL || *name == '\0' ||
		strnlen(name, MAX_TOKEN_LENGTH) >= MAX_TOKEN_LENGTH)
	{
		return false;
	}
	for (const char *p = name; *p; p++)
	{
		if (isspace((unsigned char)*p) || IsQuote(*p) || *p == '\\')
		{
			quoted = true;
		}
	}
	if (quoted)
	{
		token[used++] = '\'';
	}
	for (const char *p = name; *p; p++)
	{
		if (used + 3 >= sizeof(token))
		{
			return false;
		}
		if (quoted && (*p == '\'' || *p == '\\'))
		{
			token[used++] = '\\';
		}
		token[used++] = *p;
	}
	if (quoted)
	{
		token[used++] = '\'';
	}
	token[used] = '\0';
	int length = snprintf(output, capacity,
		"Monitor %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
		token, values[0], values[1], values[2], values[3], values[4],
		values[5], values[6], values[7], values[8], values[9],
		values[10], values[11], values[12], values[13]);
	if (length < 0 || (size_t)length >= capacity)
	{
		output[0] = '\0';
		return false;
	}
	return true;
}

bool module_monitor_prepare_area(const int values[MODULE_MONITOR_VALUES],
	const module_monitor_area *initial, bool ignore_work_area,
	module_monitor_area *result)
{
	if (result == NULL)
	{
		return false;
	}
	if (values == NULL || initial == NULL)
	{
		*result = (module_monitor_area){0};
		return false;
	}
	int64_t x = initial->x, y = initial->y;
	int64_t width = initial->width, height = initial->height;
	*result = (module_monitor_area){0};
	if (!ignore_work_area)
	{
		x += values[10];
		y += values[12];
		width -= (int64_t)values[10] + values[11];
		height -= (int64_t)values[12] + values[13];
	}
	if (x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX ||
		width <= 0 || width > INT_MAX || height <= 0 || height > INT_MAX ||
		x + width > INT_MAX || y + height > INT_MAX)
	{
		return false;
	}
	*result = (module_monitor_area){(int)x, (int)y, (int)width, (int)height};
	return true;
}
