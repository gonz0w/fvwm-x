/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "native_settings_policy.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool
decimal(const char *text, uint64_t maximum, uint64_t *value)
{
	uint64_t result = 0;

	if (!*text)
		return false;
	for (; *text; text++)
	{
		if (*text < '0' || *text > '9')
			return false;
		unsigned int digit = *text - '0';
		if (result > maximum / 10 ||
		    (result == maximum / 10 && digit > maximum % 10))
			return false;
		result = result * 10 + digit;
	}
	*value = result;
	return true;
}

static int
hex_digit(unsigned char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	return -1;
}

static bool
identity(const char *text)
{
	if (strlen(text) != 32)
		return false;
	for (size_t i = 0; i < 32; i++)
		if (hex_digit((unsigned char)text[i]) < 0)
			return false;
	return true;
}

static bool
font_valid(const unsigned char *text, size_t length)
{
	for (size_t i = 0; i < length;)
	{
		uint32_t cp = text[i++];
		unsigned int count = 0;
		uint32_t minimum = 0;

		if (cp >= 0xc2 && cp <= 0xdf)
		{
			cp &= 0x1f;
			count = 1;
			minimum = 0x80;
		}
		else if (cp >= 0xe0 && cp <= 0xef)
		{
			cp &= 0x0f;
			count = 2;
			minimum = 0x800;
		}
		else if (cp >= 0xf0 && cp <= 0xf4)
		{
			cp &= 7;
			count = 3;
			minimum = 0x10000;
		}
		else if (cp >= 0x80)
			return false;
		if (count > length - i)
			return false;
		for (unsigned int j = 0; j < count; j++)
		{
			if ((text[i] & 0xc0) != 0x80)
				return false;
			cp = (cp << 6) | (text[i++] & 0x3f);
		}
		if (cp < minimum || cp > 0x10ffff ||
		    (cp >= 0xd800 && cp <= 0xdfff) || cp < 0x20 ||
		    (cp >= 0x7f && cp <= 0x9f) || cp == 0x2028 ||
		    cp == 0x2029)
			return false;
	}
	return true;
}

static bool
theme_valid(const char *text)
{
	if (!strcmp(text, ".") || !strcmp(text, ".."))
		return false;
	for (; *text; text++)
		if (!((*text >= 'a' && *text <= 'z') ||
		    (*text >= 'A' && *text <= 'Z') ||
		    (*text >= '0' && *text <= '9') || *text == '_' ||
		    *text == '-' || *text == '.'))
			return false;
	return true;
}

static bool
decode(const char *text, char *output, bool font)
{
	size_t length = strlen(text);

	if (!strcmp(text, "-"))
	{
		output[0] = 0;
		return true;
	}
	if (!length || length % 2 || length > NS_TEXT_MAX * 2)
		return false;
	for (size_t i = 0; i < length; i += 2)
	{
		int high = hex_digit((unsigned char)text[i]);
		int low = hex_digit((unsigned char)text[i + 1]);
		if (high < 0 || low < 0 || (high == 0 && low == 0))
			return false;
		output[i / 2] = (char)((high << 4) | low);
	}
	output[length / 2] = 0;
	return font ? font_valid((unsigned char *)output, length / 2) :
	    theme_valid(output);
}

bool
ns_parse(const char *line, size_t length, ns_request *request)
{
	char buffer[NS_LINE_MAX], *fields[10];
	ns_request parsed = {0};
	uint64_t number;
	size_t count = 0;

	memset(request, 0, sizeof(*request));
	if (length < 2 || length > NS_LINE_MAX || line[length - 1] != '\n')
		return false;
	memcpy(buffer, line, length - 1);
	buffer[length - 1] = 0;
	for (size_t i = 0; i < length - 1; i++)
		if ((unsigned char)buffer[i] < 0x20 ||
		    (unsigned char)buffer[i] > 0x7e)
			return false;
	char *at = buffer;
	while (*at)
	{
		if (count == 10 || *at == ' ')
			return false;
		fields[count++] = at;
		char *space = strchr(at, ' ');
		if (!space)
			break;
		*space = 0;
		at = space + 1;
		if (!*at)
			return false;
	}
	if (count != 10 || strcmp(fields[0], "FNS1") ||
	    strcmp(fields[1], "APPLY") || !identity(fields[2]) ||
	    !identity(fields[3]) ||
	    !decimal(fields[4], UINT64_MAX, &parsed.sequence) ||
	    !parsed.sequence || !decimal(fields[5], 1, &number))
		return false;
	parsed.motion = (unsigned int)number;
	if (!decimal(fields[6], 128, &number))
		return false;
	parsed.cursor_size = (unsigned int)number;
	if (!decode(fields[7], parsed.font, true) ||
	    !decode(fields[8], parsed.icon, false) ||
	    !decode(fields[9], parsed.cursor, false))
		return false;
	memcpy(parsed.desktop, fields[2], 33);
	memcpy(parsed.run, fields[3], 33);
	*request = parsed;
	return true;
}

bool
ns_values_equal(const ns_request *a, const ns_request *b)
{
	return a->motion == b->motion && a->cursor_size == b->cursor_size &&
	    !strcmp(a->font, b->font) && !strcmp(a->icon, b->icon) &&
	    !strcmp(a->cursor, b->cursor);
}

ns_admission
ns_admit(const ns_model *model, const ns_request *request)
{
	if (!model->initialized)
		return request->sequence == 1 ? NS_ACCEPT : NS_STALE;
	if (strcmp(model->current.desktop, request->desktop) ||
	    strcmp(model->current.run, request->run) ||
	    request->sequence < model->current.sequence)
		return NS_STALE;
	if (request->sequence == model->current.sequence)
		return ns_values_equal(&model->current, request) ?
		    NS_DUPLICATE : NS_STALE;
	return NS_ACCEPT;
}

bool
ns_commit(ns_model *model, const ns_request *request)
{
	const ns_request *old = &model->current;
	bool changed[NS_KEY_COUNT] = {
		strcmp(old->font, request->font) != 0,
		strcmp(old->icon, request->icon) != 0,
		strcmp(old->cursor, request->cursor) != 0,
		old->cursor_size != request->cursor_size,
		old->motion != request->motion
	};
	bool publication = !model->initialized ||
	    !ns_values_equal(old, request);

	if (publication)
	{
		model->serial++;
		for (size_t i = 0; i < NS_KEY_COUNT; i++)
			if (!model->initialized || changed[i])
				model->changed[i] = model->serial;
	}
	model->current = *request;
	model->initialized = true;
	return publication;
}

static void
put16(unsigned char *at, uint16_t value)
{
	at[0] = value & 255;
	at[1] = value >> 8;
}

static void
put32(unsigned char *at, uint32_t value)
{
	for (unsigned int i = 0; i < 4; i++)
		at[i] = (value >> (8 * i)) & 255;
}

size_t
ns_encode(const ns_model *model, unsigned char *output, size_t capacity)
{
	static const char *names[NS_KEY_COUNT] = {
		"Gtk/FontName", "Net/IconThemeName", "Gtk/CursorThemeName",
		"Gtk/CursorThemeSize", "Gtk/EnableAnimations"
	};
	const char *strings[3] = {model->current.font, model->current.icon,
	    model->current.cursor};
	size_t used = 12;
	uint32_t count = 0;

	if (!model->initialized || capacity < 12)
		return 0;
	memset(output, 0, capacity);
	put32(output + 4, model->serial);
	for (size_t i = 0; i < NS_KEY_COUNT; i++)
	{
		bool string = i < 3;
		if ((string && !strings[i][0]) ||
		    (i == 3 && !model->current.cursor_size))
			continue;
		size_t name_length = strlen(names[i]);
		size_t padded_name = (name_length + 3) & ~(size_t)3;
		size_t text_length = string ? strlen(strings[i]) : 0;
		size_t value_length = string ?
		    4 + ((text_length + 3) & ~(size_t)3) : 4;
		size_t needed = 4 + padded_name + 4 + value_length;
		if (needed > capacity - used)
			return 0;
		output[used] = string ? 1 : 0;
		put16(output + used + 2, (uint16_t)name_length);
		memcpy(output + used + 4, names[i], name_length);
		used += 4 + padded_name;
		put32(output + used, model->changed[i]);
		used += 4;
		if (string)
		{
			put32(output + used, (uint32_t)text_length);
			memcpy(output + used + 4, strings[i], text_length);
		}
		else
			put32(output + used, i == 3 ?
			    model->current.cursor_size : model->current.motion);
		used += value_length;
		count++;
	}
	put32(output + 8, count);
	return used;
}

size_t
ns_resource_block(const ns_request *request, uint32_t owner,
	bool separator, char *output, size_t capacity)
{
	char values[192];
	size_t used = 0;
	int result;

	if (!request->cursor[0] && !request->cursor_size)
		return 0;
	if (request->cursor[0])
	{
		result = snprintf(values, sizeof(values), "Xcursor.theme: %s\n",
		    request->cursor);
		if (result < 0 || (size_t)result >= sizeof(values))
			return 0;
		used = (size_t)result;
	}
	if (request->cursor_size)
	{
		result = snprintf(values + used, sizeof(values) - used,
		    "Xcursor.size: %u\n", request->cursor_size);
		if (result < 0 || (size_t)result >= sizeof(values) - used)
			return 0;
		used += (size_t)result;
	}
	result = snprintf(output, capacity,
	    "%s! FVWM_NATIVE_SETTINGS FNS1 BEGIN %s %08" PRIx32 " %u\n"
	    "%s! FVWM_NATIVE_SETTINGS FNS1 END %s %08" PRIx32 " %u\n",
	    separator ? "\n" : "", request->run, owner, separator ? 1 : 0,
	    values, request->run, owner, separator ? 1 : 0);
	return result > 0 && (size_t)result < capacity ? (size_t)result : 0;
}

static bool
resource_boundary(const char *data, size_t length)
{
	if (length && data[length - 1] == '\n')
		length--;
	size_t slashes = 0;
	while (length && data[length - 1] == '\\')
	{
		slashes++;
		length--;
	}
	return slashes % 2 == 0;
}

ns_resource_result
ns_resource_scan(const char *data, size_t length,
	ns_block *block, bool *found)
{
	static const char marker[] = "FVWM_NATIVE_SETTINGS";
	char header[128], run[33], owner_text[9];
	unsigned int separator;
	size_t marker_at = 0, count = 0;

	memset(block, 0, sizeof(*block));
	*found = false;
	if (length > NS_RESOURCE_MAX)
		return NS_RESOURCE_LIMIT;
	if (memchr(data, 0, length))
		return NS_RESOURCE_UNSAFE;
	/* Appending a newline must not join an escaped logical line. */
	if (!resource_boundary(data, length))
		return NS_RESOURCE_UNSAFE;
	for (size_t i = 0; i + sizeof(marker) - 1 <= length; i++)
		if (!memcmp(data + i, marker, sizeof(marker) - 1))
		{
			if (!count)
				marker_at = i;
			count++;
		}
	if (!count)
		return NS_RESOURCE_OK;
	if (count != 2 || marker_at < 2 ||
	    memcmp(data + marker_at - 2, "! ", 2))
		return NS_RESOURCE_UNSAFE;
	size_t start = marker_at - 2;
	if (start && data[start - 1] != '\n')
		return NS_RESOURCE_UNSAFE;
	const char *end = memchr(data + start, '\n', length - start);
	if (!end || (size_t)(end - data - start) >= sizeof(header))
		return NS_RESOURCE_UNSAFE;
	size_t header_length = (size_t)(end - data - start);
	memcpy(header, data + start, header_length);
	header[header_length] = 0;
	static const char begin[] = "! FVWM_NATIVE_SETTINGS FNS1 BEGIN ";
	size_t fixed = sizeof(begin) - 1;
	if (header_length != fixed + 32 + 1 + 8 + 1 + 1 ||
	    memcmp(header, begin, fixed) || header[fixed + 32] != ' ' ||
	    header[fixed + 41] != ' ' ||
	    (header[fixed + 42] != '0' && header[fixed + 42] != '1'))
		return NS_RESOURCE_UNSAFE;
	memcpy(run, header + fixed, 32);
	run[32] = 0;
	memcpy(owner_text, header + fixed + 33, 8);
	owner_text[8] = 0;
	separator = (unsigned int)(header[fixed + 42] - '0');
	if (!identity(run))
		return NS_RESOURCE_UNSAFE;
	uint32_t owner = 0;
	for (size_t i = 0; i < 8; i++)
	{
		int digit = hex_digit((unsigned char)owner_text[i]);
		if (digit < 0)
			return NS_RESOURCE_UNSAFE;
		owner = (owner << 4) | (unsigned int)digit;
	}
	if (!owner || (separator && (!start || data[start - 1] != '\n')))
		return NS_RESOURCE_UNSAFE;
	memcpy(block->values.run, run, 33);
	const char *at = end + 1;
	size_t left = length - (size_t)(at - data);
	static const char theme[] = "Xcursor.theme: ";
	static const char size[] = "Xcursor.size: ";
	if (left >= sizeof(theme) - 1 &&
	    !memcmp(at, theme, sizeof(theme) - 1))
	{
		at += sizeof(theme) - 1;
		end = memchr(at, '\n', length - (size_t)(at - data));
		if (!end || end == at || (size_t)(end - at) > NS_TEXT_MAX)
			return NS_RESOURCE_UNSAFE;
		memcpy(block->values.cursor, at, (size_t)(end - at));
		if (!theme_valid(block->values.cursor))
			return NS_RESOURCE_UNSAFE;
		at = end + 1;
	}
	left = length - (size_t)(at - data);
	if (left >= sizeof(size) - 1 && !memcmp(at, size, sizeof(size) - 1))
	{
		char number[4];
		uint64_t value;
		at += sizeof(size) - 1;
		end = memchr(at, '\n', length - (size_t)(at - data));
		if (!end || end == at || end - at > 3)
			return NS_RESOURCE_UNSAFE;
		memcpy(number, at, (size_t)(end - at));
		number[end - at] = 0;
		if (!decimal(number, 128, &value) || !value)
			return NS_RESOURCE_UNSAFE;
		block->values.cursor_size = (unsigned int)value;
		at = end + 1;
	}
	char canonical[NS_BLOCK_MAX];
	size_t block_length = ns_resource_block(&block->values, owner,
	    separator != 0, canonical, sizeof(canonical));
	if (separator)
		start--;
	if (!resource_boundary(data, start) || !block_length ||
	    block_length > length - start ||
	    memcmp(data + start, canonical, block_length))
		return NS_RESOURCE_UNSAFE;
	block->start = start;
	block->length = block_length;
	block->owner = owner;
	block->separator = separator != 0;
	memcpy(block->run, run, 33);
	*found = true;
	return NS_RESOURCE_OK;
}

bool
ns_resource_owned(const char *data, size_t length, const char *owned,
	size_t owned_length, bool cleanup, size_t *start)
{
	ns_block block;
	bool found;

	*start = 0;
	if (!owned_length || ns_resource_scan(data, length, &block, &found) !=
	    NS_RESOURCE_OK || !found || block.length != owned_length ||
	    memcmp(data + block.start, owned, owned_length) ||
	    (!cleanup && block.start + owned_length != length))
		return false;
	*start = block.start;
	return true;
}
