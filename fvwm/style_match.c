/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <string.h>
#include "fvwm.h"
#include "style_match.h"
#include "libs/wild.h"

void style_match_prepare(style_match_patterns *patterns, const style_id_t *id)
{
	const char *values[STYLE_MATCH_FIELDS] = {
		id->resource, id->class, id->name, id->icon
	};
	bool active[STYLE_MATCH_FIELDS] = {
		id->flags.has_resource, id->flags.has_class,
		id->flags.has_name || id->flags.is_compatibility_mode,
		id->flags.has_icon
	};

	memset(patterns, 0, sizeof(*patterns));
	for (unsigned i = 0; i < STYLE_MATCH_FIELDS; i++)
	{
		const char *value = values[i];
		unsigned bit = 1U << i;

		if (!active[i])
			continue;
		/* Preserve the wildcard owner's NULL and exact-"*" semantics.
		 * Any escape stays on its original parser/matcher path. */
		if (!value || (value[0] == '*' && value[1] == '\0'))
		{
			patterns->universal |= bit;
			continue;
		}
		patterns->prefix[i] = strcspn(value, "*?\\");
		if (value[patterns->prefix[i]] == '\0')
			patterns->literal |= bit;
	}
}

void style_match_subject_init(style_match_subject *subject, Window window,
	const char *resource, const char *class, const char *name, const char *icon)
{
	subject->window = window;
	subject->value[STYLE_MATCH_RESOURCE] = resource;
	subject->value[STYLE_MATCH_CLASS] = class;
	subject->value[STYLE_MATCH_NAME] = name;
	subject->value[STYLE_MATCH_ICON] = icon;
	subject->compatibility_fields = 0;
	/* Equality is computed once per lookup, not once per rule. Keep the
	 * first representative, including NULL, in original evaluation order. */
	for (unsigned i = 0; i <= STYLE_MATCH_NAME; i++)
	{
		unsigned j;

		for (j = 0; j < i; j++)
		{
			if (subject->value[i] == subject->value[j] ||
			    (subject->value[i] && subject->value[j] &&
			     strcmp(subject->value[i], subject->value[j]) == 0))
				break;
		}
		if (j == i)
			subject->compatibility_fields |= 1U << i;
	}
}

static bool match_pattern(const char *pattern,
	const style_match_patterns *patterns, unsigned field, const char *value)
{
	unsigned bit = 1U << field;

	if (patterns->universal & bit)
		return true;
	if (!value)
		return false;
	if (patterns->literal & bit)
		return strcmp(pattern, value) == 0;
	/* This span contains no wildcard or escape. A mismatch cannot be
	 * repaired by the suffix; successful prefixes still use the owner. */
	if (strncmp(pattern, value, patterns->prefix[field]) != 0)
		return false;
	return matchWildcards(pattern, value) != 0;
}

bool style_match_id(const style_id_t *id, const style_match_patterns *patterns,
	const style_match_subject *subject)
{
	/* A numeric mismatch decides this conjunction without string work. */
	if (id->flags.has_window_id && id->window_id != subject->window)
		return false;
	if (id->flags.is_compatibility_mode)
	{
		unsigned i;

		for (i = 0; i <= STYLE_MATCH_NAME; i++)
		{
			if ((subject->compatibility_fields & (1U << i)) &&
			    match_pattern(id->name, patterns, STYLE_MATCH_NAME,
				subject->value[i]))
				break;
		}
		if (i > STYLE_MATCH_NAME)
			return false;
	}
	if (id->flags.has_resource &&
	    !match_pattern(id->resource, patterns, STYLE_MATCH_RESOURCE,
		subject->value[STYLE_MATCH_RESOURCE]))
		return false;
	if (id->flags.has_class &&
	    !match_pattern(id->class, patterns, STYLE_MATCH_CLASS,
		subject->value[STYLE_MATCH_CLASS]))
		return false;
	if (id->flags.has_name &&
	    !match_pattern(id->name, patterns, STYLE_MATCH_NAME,
		subject->value[STYLE_MATCH_NAME]))
		return false;
	if (id->flags.has_icon &&
	    !match_pattern(id->icon, patterns, STYLE_MATCH_ICON,
		subject->value[STYLE_MATCH_ICON]))
		return false;
	return true;
}
