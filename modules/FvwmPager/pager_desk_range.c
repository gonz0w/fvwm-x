/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "pager_desk_range.h"
#include "libs/module_monitor.h"
#include <stdint.h>
#include <stddef.h>

bool pager_prepare_desk_range(const char *first_text, const char *last_text,
	int limit, pager_desk_range *result)
{
	int first, last;
	if (result == NULL)
	{
		return false;
	}
	*result = (pager_desk_range){0};
	if (limit < 1 || limit > 65536 ||
		!module_parse_decimal_integers(first_text, &first, 1))
	{
		return false;
	}
	if (last_text == NULL)
	{
		last = first;
	}
	else if (!module_parse_decimal_integers(last_text, &last, 1))
	{
		return false;
	}
	if (first < 0)
	{
		first = 0;
	}
	if (last < 0)
	{
		last = 0;
	}
	if (first > last)
	{
		int swap = first;
		first = last;
		last = swap;
	}
	int64_t count = (int64_t)last - first + 1;
	if (count > limit)
	{
		return false;
	}
	*result = (pager_desk_range){first, last, (int)count};
	return true;
}

int pager_desk_index(int desk, int first, int count)
{
	int64_t index = (int64_t)desk - first;

	if (count <= 0 || index < 0 || index >= count)
	{
		return -1;
	}
	return (int)index;
}
