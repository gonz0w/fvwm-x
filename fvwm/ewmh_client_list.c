/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ewmh_client_list.h"

bool ewmh_client_list_grow_capacity(size_t capacity, size_t *grown)
{
	size_t limit = SIZE_MAX / sizeof(Window);

	if (limit > INT_MAX)
		limit = INT_MAX;
	*grown = 0;
	if (capacity >= limit)
		return false;
	if (capacity < EWMH_CLIENT_LIST_INLINE)
		*grown = limit < EWMH_CLIENT_LIST_INLINE ?
			limit : EWMH_CLIENT_LIST_INLINE;
	else
		*grown = capacity > limit / 2 ? limit : capacity * 2;
	return true;
}

void ewmh_client_list_init(ewmh_client_list *list)
{
	list->windows = list->local;
	list->count = 0;
	list->capacity = EWMH_CLIENT_LIST_INLINE;
}

bool ewmh_client_list_append(ewmh_client_list *list, Window window)
{
	if (list->count == list->capacity)
	{
		size_t capacity;
		Window *windows;

		if (!ewmh_client_list_grow_capacity(list->capacity, &capacity))
			return false;
		if (list->windows == list->local)
		{
			windows = malloc(capacity * sizeof(*windows));
			if (windows != NULL)
				memcpy(windows, list->local,
					list->count * sizeof(*windows));
		}
		else
			windows = realloc(list->windows,
				capacity * sizeof(*windows));
		if (windows == NULL)
			return false;
		list->windows = windows;
		list->capacity = capacity;
	}
	list->windows[list->count++] = window;
	return true;
}

void ewmh_client_list_finish(ewmh_client_list *list)
{
	if (list->windows != list->local)
		free(list->windows);
	ewmh_client_list_init(list);
}
