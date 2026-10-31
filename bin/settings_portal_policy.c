/* This program is free software; you can redistribute it and/or modify
 * it under the GNU General Public License as published by the Free Software
 * Foundation; either version 2, or any later version. */
#include "config.h"
#include "settings_portal_policy.h"
#include <string.h>

static uint16_t
read16(const unsigned char *data, bool big)
{
	return big ? ((uint16_t)data[0] << 8) | data[1] :
	    ((uint16_t)data[1] << 8) | data[0];
}

static uint32_t
read32(const unsigned char *data, bool big)
{
	uint32_t value = 0;

	for (size_t i = 0; i < 4; i++)
		value |= (uint32_t)data[i] << (8 * (big ? 3 - i : i));
	return value;
}

static bool
skip(size_t *offset, size_t count, size_t length)
{
	if (count > length - *offset)
		return false;
	*offset += count;
	return true;
}

static bool
align4(size_t *offset, size_t length)
{
	return skip(offset, (4 - *offset % 4) % 4, length);
}

bool
sp_decode(const unsigned char *data, size_t length, sp_snapshot *snapshot)
{
	static const char target[] = "Gtk/EnableAnimations";
	sp_snapshot decoded = {0};
	size_t offset = 12;
	bool found = false;

	memset(snapshot, 0, sizeof(*snapshot));
	if (!data || length < 12 || length > SP_SETTINGS_MAX || data[0] > 1)
		return false;
	bool big = data[0] == 1;
	uint32_t records = read32(data + 8, big);
	if (records > SP_RECORDS_MAX)
		return false;
	decoded.serial = read32(data + 4, big);
	for (uint32_t i = 0; i < records; i++)
	{
		if (!skip(&offset, 4, length))
			return false;
		unsigned char type = data[offset - 4];
		size_t name_length = read16(data + offset - 2, big);
		const unsigned char *name = data + offset;
		if (!name_length || !skip(&offset, name_length, length) ||
		    memchr(name, 0, name_length) || !align4(&offset, length) ||
		    !skip(&offset, 4, length))
			return false;
		bool match = name_length == sizeof(target) - 1 &&
		    memcmp(name, target, name_length) == 0;
		if (match && (found || type != 0))
			return false;
		if (type == 0)
		{
			if (!skip(&offset, 4, length))
				return false;
			uint32_t value = read32(data + offset - 4, big);
			if (match)
			{
				if (value > 1)
					return false;
				decoded.reduced_motion = value == 0 ? 1 : 0;
				found = true;
			}
		}
		else if (type == 1)
		{
			if (!skip(&offset, 4, length))
				return false;
			uint32_t bytes = read32(data + offset - 4, big);
			if (!skip(&offset, bytes, length) ||
			    !align4(&offset, length))
				return false;
		}
		else if (type == 2)
		{
			if (!skip(&offset, 8, length))
				return false;
		}
		else
			return false;
	}
	if (offset != length)
		return false;
	*snapshot = decoded;
	return true;
}

bool
sp_pattern_valid(const char *pattern)
{
	if (!pattern)
		return false;
	size_t length = strnlen(pattern, SP_PATTERN_MAX + 1);
	if (length > SP_PATTERN_MAX)
		return false;
	if (length == 0 || strcmp(pattern, "*") == 0)
		return true;
	for (size_t i = 0; i < length; i++)
	{
		unsigned char byte = (unsigned char)pattern[i];

		if (byte == '*')
			return i == length - 1 && i > 1 &&
			    pattern[i - 1] == '.';
		if (byte < 0x21 || byte > 0x7e)
			return false;
	}
	return true;
}

bool
sp_namespace_matches(const char *const *patterns, size_t count)
{
	bool selected = count == 0;

	if (count > SP_PATTERNS_MAX || (count && !patterns))
		return false;
	for (size_t i = 0; i < count; i++)
	{
		const char *pattern = patterns[i];

		/* Validate the entire array, even after a matching member. */
		if (!sp_pattern_valid(pattern))
			return false;
		size_t length = strlen(pattern);
		if (!length || strcmp(pattern, "*") == 0 ||
		    strcmp(pattern, SP_NAMESPACE) == 0 ||
		    (pattern[length - 1] == '*' &&
		    strncmp(pattern, SP_NAMESPACE, length - 1) == 0))
			selected = true;
	}
	return selected;
}

bool
sp_commit(uint32_t *current, const sp_snapshot *snapshot)
{
	uint32_t value = snapshot ? snapshot->reduced_motion : 0;
	bool changed = value != *current;

	*current = value;
	return changed;
}
