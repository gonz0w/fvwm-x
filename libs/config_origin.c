/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "config_origin.h"
#include "safemalloc.h"
#include "font_text.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fvwm_config_origin
{
	size_t refs;
	size_t first, last;
	unsigned int depth;
	bool file_chain;
	char *path;
	const fvwm_config_origin *parent;
	char json[FVWM_CONFIG_SOURCE_MAX_BYTES + 1];
};

static fvwm_config_origin unavailable_origin = { .json =
	"{\"version\":1,\"kind\":\"unavailable\",\"reason\":"
	"\"source unavailable or exceeds provenance bounds\"}" };

static fvwm_config_origin generated_origin = { .json =
	"{\"version\":1,\"kind\":\"generated\",\"reason\":"
	"\"generated command; invocation source unavailable\"}" };
static fvwm_config_origin invocation_origin = { .json =
	"{\"version\":1,\"kind\":\"invocation\",\"reason\":"
	"\"function definition and invocation source unavailable\"}" };

/* Reject malformed UTF-8 instead of emitting a lossy, apparently exact
 * path. */
static bool valid_utf8(const unsigned char *s)
{
	size_t remaining = strlen((const char *)s);
	while (remaining > 0)
	{
		uint32_t scalar;
		size_t count = font_text_utf8(s, remaining, &scalar);
		if ((count == 1 && *s >= 0x80) || scalar < 32 ||
		    (scalar >= 0x7f && scalar <= 0x9f))
			return false;
		s += count;
		remaining -= count;
	}
	return true;
}

static bool append(char *out, size_t *used, const char *s)
{
	size_t n = strlen(s);
	if (n > FVWM_CONFIG_SOURCE_MAX_BYTES - *used)
		return false;
	memcpy(out + *used, s, n + 1);
	*used += n;
	return true;
}

static bool quoted(char *out, size_t *used, const char *s)
{
	const unsigned char *p = (const unsigned char *)s;
	char escaped[7];
	if (!valid_utf8(p) || !append(out, used, "\""))
		return false;
	for (; *p; p++)
	{
		if (*p < 32 || *p == '"' || *p == '\\')
			snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
		else
			escaped[0] = *p, escaped[1] = 0;
		if (!append(out, used, escaped))
			return false;
	}
	return append(out, used, "\"");
}

static bool location(char *out, size_t *used,
	const fvwm_config_origin *origin)
{
	char numbers[96];
	snprintf(numbers, sizeof(numbers), ",\"first\":%zu,\"last\":%zu",
		origin->first, origin->last);
	return append(out, used, "\"path\":") &&
		quoted(out, used, origin->path) && append(out, used, numbers);
}

static bool serialize(fvwm_config_origin *origin, const char *kind,
	const char *reason)
{
	const fvwm_config_origin *chain[FVWM_CONFIG_SOURCE_MAX_INCLUDE];
	const fvwm_config_origin *p;
	size_t used = 0;
	unsigned int n = 0;
	char *out = origin->json;
	out[0] = 0;
	if (!append(out, &used, "{\"version\":1,\"kind\":") ||
	    !quoted(out, &used, kind) || !append(out, &used, ",") ||
	    !location(out, &used, origin) ||
	    !append(out, &used, ",\"include\":["))
		return false;
	for (p = origin->parent; p != NULL; p = p->parent)
	{
		if (n == FVWM_CONFIG_SOURCE_MAX_INCLUDE || p->path == NULL)
			return false;
		chain[n++] = p;
	}
	while (n > 0)
	{
		if (!append(out, &used, "{") ||
		    !location(out, &used, chain[--n]) ||
		    !append(out, &used, n ? "}," : "}"))
			return false;
	}
	return append(out, &used, "],\"reason\":") &&
		quoted(out, &used, reason) && append(out, &used, "}");
}

static fvwm_config_origin *make_origin(const char *path, size_t first,
	size_t last, const fvwm_config_origin *parent, const char *kind,
	const char *reason)
{
	fvwm_config_origin *origin;
	if (path == NULL || *path == 0 || first == 0 || last < first ||
	    last > UINT32_MAX ||
	    strlen(path) > FVWM_CONFIG_SOURCE_MAX_BYTES ||
	    (parent != NULL && (parent->path == NULL ||
	    parent->depth > FVWM_CONFIG_SOURCE_MAX_INCLUDE)))
		return &unavailable_origin;
	origin = fxcalloc(1, sizeof(*origin));
	origin->refs = 1;
	origin->path = fxstrdup(path);
	origin->first = first;
	origin->last = last;
	origin->depth = parent ? parent->depth + 1 : 1;
	origin->file_chain = strcmp(kind, "file") == 0;
	origin->parent = fvwm_config_origin_retain(parent);
	if (!serialize(origin, kind, reason))
	{
		fvwm_config_origin_release(origin);
		return &unavailable_origin;
	}
	return origin;
}

fvwm_config_origin *fvwm_config_origin_file(const char *path,
	size_t first, size_t last, const fvwm_config_origin *parent)
{
	/* Only absolute paths can claim a complete file origin. */
	if (path == NULL || path[0] != '/' ||
	    (parent != NULL && !parent->file_chain))
		return &unavailable_origin;
	return make_origin(path, first, last, parent, "file", "");
}

fvwm_config_origin *fvwm_config_origin_derive(
	const fvwm_config_origin *site, const char *kind, const char *reason)
{
	if (strcmp(kind, "generated") != 0 && strcmp(kind, "invocation") != 0)
		return &unavailable_origin;
	if (site == NULL || site->path == NULL)
		return strcmp(kind, "generated") == 0 ?
			&generated_origin : &invocation_origin;
	return make_origin(site->path, site->first, site->last, site->parent,
		kind, reason);
}

fvwm_config_origin *fvwm_config_origin_retain(const fvwm_config_origin *origin)
{
	if (origin != NULL && origin != &unavailable_origin &&
	    origin != &generated_origin && origin != &invocation_origin)
		((fvwm_config_origin *)origin)->refs++;
	return (fvwm_config_origin *)origin;
}

void fvwm_config_origin_release(const fvwm_config_origin *origin)
{
	fvwm_config_origin *owned = (fvwm_config_origin *)origin;
	if (owned != NULL && owned != &unavailable_origin &&
	    owned != &generated_origin && owned != &invocation_origin &&
	    --owned->refs == 0)
	{
		fvwm_config_origin_release(owned->parent);
		free(owned->path);
		free(owned);
	}
}

const char *fvwm_config_origin_json(const fvwm_config_origin *origin)
{
	return origin ? origin->json : unavailable_origin.json;
}
