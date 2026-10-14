/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "script_index.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

bool script_index_parse(const char *text, int minimum, int maximum, int *value)
{
	char *end;
	long parsed;

	*value = 0;
	if (text == NULL || minimum > maximum)
		return false;
	errno = 0;
	parsed = strtol(text, &end, 10);
	if (errno != 0 || end == text || *end != '\0' ||
	    parsed < minimum || parsed > maximum)
		return false;
	*value = (int)parsed;
	return true;
}

int script_widget_index(const char *text, const int *ids, size_t id_count,
	size_t object_count)
{
	int id;

	if (ids == NULL || id_count < 2 || id_count - 1 > INT_MAX ||
	    !script_index_parse(text, 1, (int)(id_count - 1), &id) ||
	    ids[id] < 0 || (size_t)ids[id] >= object_count)
		return -1;
	return ids[id];
}
