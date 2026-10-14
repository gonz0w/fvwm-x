/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "colorset_store.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t lower_bound(const colorset_store *store, int id)
{
	size_t first = 0, last = store->count;

	while (first < last)
	{
		size_t middle = first + (last - first) / 2;
		if (store->entries[middle].id < id)
		{
			first = middle + 1;
		}
		else
		{
			last = middle;
		}
	}
	return first;
}

void *colorset_store_find(const colorset_store *store, int id)
{
	if (id < 0)
	{
		return NULL;
	}
	size_t position = lower_bound(store, id);
	return position < store->count && store->entries[position].id == id ?
		store->entries[position].record : NULL;
}

void *colorset_store_ensure(colorset_store *store, int id, bool *created)
{
	*created = false;
	if (id < 0)
	{
		return NULL;
	}
	size_t position = lower_bound(store, id);
	if (position < store->count && store->entries[position].id == id)
	{
		return store->entries[position].record;
	}
	if (store->count >= store->limit || store->record_size == 0 ||
	    store->limit > SIZE_MAX / sizeof(*store->entries))
	{
		return NULL;
	}
	void *record = calloc(1, store->record_size);
	if (record == NULL)
	{
		return NULL;
	}
	if (store->count == store->capacity)
	{
		size_t capacity = store->capacity > store->limit / 2 ?
			store->limit : store->capacity * 2;
		if (capacity < 16)
		{
			capacity = store->limit < 16 ? store->limit : 16;
		}
		colorset_store_entry *entries = realloc(store->entries,
			capacity * sizeof(*entries));
		if (entries == NULL)
		{
			free(record);
			return NULL;
		}
		store->entries = entries;
		store->capacity = capacity;
	}
	memmove(store->entries + position + 1, store->entries + position,
		(store->count - position) * sizeof(*store->entries));
	store->entries[position] = (colorset_store_entry){id, record};
	store->count++;
	*created = true;
	return record;
}

int colorset_store_id_at(const colorset_store *store, size_t position)
{
	return position < store->count ? store->entries[position].id : -1;
}

void colorset_store_destroy(colorset_store *store)
{
	for (size_t i = 0; i < store->count; i++)
	{
		free(store->entries[i].record);
	}
	free(store->entries);
	store->entries = NULL;
	store->count = store->capacity = 0;
}

bool colorset_store_parse_id(const char *text, int base, int *id, char **end)
{
	*id = 0;
	if (end != NULL)
	{
		*end = (char *)text;
	}
	if (text == NULL || (base != 10 && base != 16))
	{
		return false;
	}
	char *tail;
	errno = 0;
	long value = strtol(text, &tail, base);
	if (tail == text || errno == ERANGE || value < INT32_MIN ||
	    value > INT32_MAX || value < INT_MIN || value > INT_MAX)
	{
		return false;
	}
	*id = (int)value;
	if (end != NULL)
	{
		*end = tail;
	}
	return true;
}
