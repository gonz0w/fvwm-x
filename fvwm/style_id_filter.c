/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <stddef.h>
#include "style_id_filter.h"

static uint64_t hash_byte(uint64_t hash, unsigned char value)
{
	return (hash ^ value) * UINT64_C(1099511628211);
}

static uint64_t hash_string(uint64_t hash, const char *value)
{
	/* Valid active selectors own non-NULL strings, as required by the
	 * equality/matching owner. Include a terminator between fields. */
	do
	{
		hash = hash_byte(hash, (unsigned char)*value);
	} while (*value++);
	return hash;
}

static uint64_t hash_id(const style_id_t *id)
{
	uint64_t hash = UINT64_C(14695981039346656037);
	unsigned flags = id->flags.has_resource |
		(id->flags.has_class << 1) | (id->flags.has_name << 2) |
		(id->flags.has_icon << 3) | (id->flags.has_window_id << 4) |
		(id->flags.is_compatibility_mode << 5);

	hash = hash_byte(hash, flags);
	if (id->flags.has_resource)
		hash = hash_string(hash, id->resource);
	if (id->flags.has_class)
		hash = hash_string(hash, id->class);
	if (id->flags.has_name || id->flags.is_compatibility_mode)
		hash = hash_string(hash, id->name);
	if (id->flags.has_icon)
		hash = hash_string(hash, id->icon);
	if (id->flags.has_window_id)
	{
		uint64_t window = id->window_id;
		for (unsigned i = 0; i < 8; i++, window >>= 8)
			hash = hash_byte(hash, window & 0xff);
	}
	return hash;
}

void style_id_filter_add(style_id_filter *filter, const style_id_t *id)
{
	uint64_t hash = hash_id(id);

	for (unsigned i = 0; i < 2; i++, hash >>= 32)
	{
		size_t slot = hash & 8191;
		uint64_t bit = UINT64_C(1) << (slot & 63);

		filter->repeated[i][slot >> 6] |=
			filter->seen[i][slot >> 6] & bit;
		filter->seen[i][slot >> 6] |= bit;
	}
}

bool style_id_filter_may_repeat(const style_id_filter *filter,
	const style_id_t *id)
{
	uint64_t hash = hash_id(id);

	for (unsigned i = 0; i < 2; i++, hash >>= 32)
	{
		size_t slot = hash & 8191;
		uint64_t bit = UINT64_C(1) << (slot & 63);

		if (!(filter->repeated[i][slot >> 6] & bit))
			return false;
	}
	return true;
}
