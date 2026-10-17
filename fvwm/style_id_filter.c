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
	if (!value)
		return hash_byte(hash, 0);
	/* Valid active selectors own non-NULL strings, as required by the
	 * equality/matching owner. Include a terminator between fields. */
	do
	{
		hash = hash_byte(hash, (unsigned char)*value);
	} while (*value++);
	return hash;
}

uint64_t style_id_hash(const style_id_t *id)
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

void style_id_filter_record(style_id_filter *filter, uint64_t hash,
	size_t ordinal)
{
	for (unsigned i = 0; i < 2; i++, hash >>= 32)
	{
		size_t slot = hash & 8191;
		uint64_t bit = UINT64_C(1) << (slot & 63);
		size_t first_slot = hash & 1023;

		if (ordinal && !filter->first[i][first_slot])
			filter->first[i][first_slot] = ordinal > UINT16_MAX ?
				UINT16_MAX : ordinal;
		filter->repeated[i][slot >> 6] |=
			filter->seen[i][slot >> 6] & bit;
		filter->seen[i][slot >> 6] |= bit;
	}
}

bool style_id_filter_hash_may_repeat(const style_id_filter *filter,
	uint64_t hash)
{
	for (unsigned i = 0; i < 2; i++, hash >>= 32)
	{
		size_t slot = hash & 8191;
		uint64_t bit = UINT64_C(1) << (slot & 63);

		if (!(filter->repeated[i][slot >> 6] & bit))
			return false;
	}
	return true;
}

size_t style_id_filter_first(const style_id_filter *filter, uint64_t hash)
{
	size_t first = 0;

	/* Each bucket may include earlier unrelated IDs. The later of two
	 * such lower bounds still cannot pass the first actual equal ID. */
	for (unsigned i = 0; i < 2; i++, hash >>= 32)
	{
		size_t ordinal = filter->first[i][hash & 1023];

		if (ordinal > first)
			first = ordinal;
	}
	return first;
}

void style_id_filter_add(style_id_filter *filter, const style_id_t *id)
{
	style_id_filter_record(filter, style_id_hash(id), 1);
}

bool style_id_filter_may_repeat(const style_id_filter *filter,
	const style_id_t *id)
{
	return style_id_filter_hash_may_repeat(filter, style_id_hash(id));
}
