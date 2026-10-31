/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "background_plan.h"
#include <stdio.h>
#include <string.h>

static const char alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int digit(char c)
{
	const char *p = c ? strchr(alphabet, c) : NULL;

	return p ? (int)(p - alphabet) : -1;
}

bool background_plan_format(char *output, size_t capacity,
	const struct background_plan *plan)
{
	char path[345] = "-";
	size_t length, offset = 0;
	int n;

	if (!output || !capacity)
		return false;
	*output = '\0';
	if (!plan || plan->flags > 15 ||
	    (plan->flags & BACKGROUND_MODE_MASK) == BACKGROUND_MODE_MASK ||
	    plan->menu_opacity > 100 || plan->launcher_opacity > 100 ||
	    plan->rgb > 0xffffff)
		return false;
	length = strnlen(plan->path, sizeof(plan->path));
	if (((plan->flags & BACKGROUND_MODE_MASK) == 4 &&
	     (!length || length > BACKGROUND_PATH_MAX)) ||
	    ((plan->flags & BACKGROUND_MODE_MASK) != 4 && length))
		return false;
	if (length)
	{
		for (size_t i = 0; i < length; i += 3)
		{
			unsigned a = (unsigned char)plan->path[i];
			unsigned b = i + 1 < length ?
				(unsigned char)plan->path[i + 1] : 0;
			unsigned c = i + 2 < length ?
				(unsigned char)plan->path[i + 2] : 0;

			path[offset++] = alphabet[a >> 2];
			path[offset++] = alphabet[((a & 3) << 4) | (b >> 4)];
			path[offset++] = i + 1 < length ?
				alphabet[((b & 15) << 2) | (c >> 6)] : '=';
			path[offset++] = i + 2 < length ? alphabet[c & 63] : '=';
		}
		path[offset] = '\0';
	}
	n = snprintf(output, capacity, "%02x:%03u:%03u:%06x:%s",
		plan->flags, plan->menu_opacity, plan->launcher_opacity,
		plan->rgb, path);
	if (n < 0 || (size_t)n >= capacity)
	{
		*output = '\0';
		return false;
	}
	return true;
}

bool background_plan_parse(const char *text, struct background_plan *plan)
{
	struct background_plan parsed = {0};
	char canonical[BACKGROUND_SUFFIX_MAX + 1];
	const char *path;
	size_t length, offset = 0;

	if (!plan)
		return false;
	memset(plan, 0, sizeof(*plan));
	if (!text || strnlen(text, BACKGROUND_SUFFIX_MAX + 2) >
	    BACKGROUND_SUFFIX_MAX || strlen(text) < 19 || text[2] != ':' ||
	    text[6] != ':' || text[10] != ':' || text[17] != ':' ||
	    strspn(text, "0123456789abcdef") != 2 ||
	    strspn(text + 3, "0123456789") != 3 ||
	    strspn(text + 7, "0123456789") != 3 ||
	    strspn(text + 11, "0123456789abcdef") != 6 ||
	    sscanf(text, "%2x:%3u:%3u:%6x", &parsed.flags,
		&parsed.menu_opacity, &parsed.launcher_opacity,
		&parsed.rgb) != 4)
		return false;
	path = text + 18;
	length = strlen(path);
	if (strcmp(path, "-") != 0)
	{
		if (!length || length > 344 || length % 4)
			return false;
		for (size_t i = 0; i < length; i += 4)
		{
			int a = digit(path[i]), b = digit(path[i + 1]);
			int c = path[i + 2] == '=' ? 0 : digit(path[i + 2]);
			int d = path[i + 3] == '=' ? 0 : digit(path[i + 3]);
			unsigned bytes[3];
			unsigned count = path[i + 2] == '=' ? 1 :
				path[i + 3] == '=' ? 2 : 3;

			if (a < 0 || b < 0 || c < 0 || d < 0 ||
			    (count != 3 && i + 4 != length) ||
			    (count == 1 && (path[i + 3] != '=' || (b & 15))) ||
			    (count == 2 && (c & 3)) ||
			    offset + count > BACKGROUND_PATH_MAX)
				return false;
			bytes[0] = (a << 2) | (b >> 4);
			bytes[1] = ((b & 15) << 4) | (c >> 2);
			bytes[2] = ((c & 3) << 6) | d;
			for (unsigned j = 0; j < count; j++)
			{
				if (!bytes[j])
					return false;
				parsed.path[offset++] = (char)bytes[j];
			}
		}
	}
	if (!background_plan_format(canonical, sizeof(canonical), &parsed) ||
	    strcmp(canonical, text) != 0)
		return false;
	*plan = parsed;
	return true;
}
