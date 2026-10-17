/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <string.h>
#include "fvwm.h"
#include "style_match.h"
#include "style_id_filter.h"
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
	patterns->fingerprint = style_id_hash(id);
	for (unsigned i = 0; i < STYLE_MATCH_FIELDS; i++)
	{
		const char *value = values[i];
		size_t length = 0, stars = 0, questions = 0;
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
		{
			patterns->literal |= bit;
			continue;
		}
		/* Escape parsing, including a trailing backslash, remains wholly
		 * with matchWildcards. Unescaped '?' consumes one byte, not one
		 * Unicode character; '*' alone can consume zero bytes. */
		if (strchr(value, '\\'))
			continue;
		patterns->unescaped |= bit;

		for (; value[length]; length++)
		{
			if (value[length] == '*')
				stars++;
			else
				patterns->minimum[i]++;
			if (value[length] == '?')
				questions++;
			if (value[length] == '*' || value[length] == '?')
				patterns->suffix[i] = length + 1;
		}
		patterns->suffix_length[i] = length - patterns->suffix[i];
		if (stars == 1 && questions == 0)
			patterns->single_star |= bit;
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
	subject->length_fields = 0;
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
	const style_match_patterns *patterns, unsigned field,
	style_match_subject *subject, unsigned subject_field)
{
	unsigned bit = 1U << field;
	const char *value = subject->value[subject_field];

	if (patterns->universal & bit)
		return true;
	if (!value)
		return false;
	if (patterns->literal & bit)
		return strcmp(pattern, value) == 0;
	/* This span contains no wildcard or escape. A mismatch cannot be
	 * repaired by the suffix; successful prefixes need the remaining checks. */
	if (strncmp(pattern, value, patterns->prefix[field]) != 0)
		return false;
	if (patterns->unescaped & bit)
	{
		unsigned subject_bit = 1U << subject_field;
		size_t suffix_length = patterns->suffix_length[field];
		size_t length;

		/* A trailing single star needs no full-value scan: the prefix
		 * check already proves its entire minimum length. All-star
		 * general patterns have no length/suffix test to perform. */
		if ((patterns->single_star & bit) && suffix_length == 0)
			return true;
		if (patterns->minimum[field] == 0 && suffix_length == 0)
			return matchWildcards(pattern, value) != 0;
		if (!(subject->length_fields & subject_bit))
		{
			subject->length[subject_field] = strlen(value);
			subject->length_fields |= subject_bit;
		}
		length = subject->length[subject_field];
		if (length < patterns->minimum[field])
			return false;

		/* minimum includes every suffix byte, so this subtraction cannot
		 * underflow. Only the final literal span is anchored at the end. */
		if (suffix_length && memcmp(pattern + patterns->suffix[field],
		    value + length - suffix_length, suffix_length) != 0)
			return false;
		/* Exactly one unescaped star and no '?' leaves only the two
		 * verified spans. minimum prevents prefix/suffix overlap. Keep
		 * multi-star patterns with the legacy matcher: e.g. "**" does
		 * not match an empty value under that owner's current semantics. */
		if (patterns->single_star & bit)
			return true;
	}
	return matchWildcards(pattern, value) != 0;
}

bool style_match_id(const style_id_t *id, const style_match_patterns *patterns,
	style_match_subject *subject)
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
				subject, i))
				break;
		}
		if (i > STYLE_MATCH_NAME)
			return false;
	}
	if (id->flags.has_resource &&
	    !match_pattern(id->resource, patterns, STYLE_MATCH_RESOURCE,
		subject, STYLE_MATCH_RESOURCE))
		return false;
	if (id->flags.has_class &&
	    !match_pattern(id->class, patterns, STYLE_MATCH_CLASS,
		subject, STYLE_MATCH_CLASS))
		return false;
	if (id->flags.has_name &&
	    !match_pattern(id->name, patterns, STYLE_MATCH_NAME,
		subject, STYLE_MATCH_NAME))
		return false;
	if (id->flags.has_icon &&
	    !match_pattern(id->icon, patterns, STYLE_MATCH_ICON,
		subject, STYLE_MATCH_ICON))
		return false;
	return true;
}

static bool pattern_spans_may_overlap(const char *a,
	const style_match_patterns *ap, const char *b,
	const style_match_patterns *bp, unsigned field)
{
	unsigned bit = 1U << field;
	size_t common, alength, blength;
	const char *asuffix, *bsuffix;

	if ((ap->universal | bp->universal) & bit)
		return true;
	common = ap->prefix[field] < bp->prefix[field] ?
		ap->prefix[field] : bp->prefix[field];
	if (common && memcmp(a, b, common) != 0)
		return false;
	if ((ap->literal & bp->literal & bit) &&
	    ap->prefix[field] != bp->prefix[field])
		return false;
	/* An escaped pattern has no prepared anchored suffix. Its initial
	 * unescaped prefix remains necessary; everything else is uncertain. */
	alength = ap->literal & bit ? ap->prefix[field] :
		ap->unescaped & bit ? ap->suffix_length[field] : 0;
	blength = bp->literal & bit ? bp->prefix[field] :
		bp->unescaped & bit ? bp->suffix_length[field] : 0;
	common = alength < blength ? alength : blength;
	if (!common)
		return true;
	asuffix = a + (ap->literal & bit ? 0 : ap->suffix[field]);
	bsuffix = b + (bp->literal & bit ? 0 : bp->suffix[field]);
	return memcmp(asuffix + alength - common,
		bsuffix + blength - common, common) == 0;
}

/* False proves that no window can satisfy both conjunctions. Compatibility
 * selectors OR resource/class/name, so their name spans alone prove nothing.
 * Only constraints on the same explicit field can establish disjointness. */
bool style_match_ids_may_overlap(const style_id_t *a,
	const style_match_patterns *ap, const style_id_t *b,
	const style_match_patterns *bp)
{
	const char *av[STYLE_MATCH_FIELDS] = {
		a->resource, a->class, a->name, a->icon
	};
	const char *bv[STYLE_MATCH_FIELDS] = {
		b->resource, b->class, b->name, b->icon
	};
	bool shared[STYLE_MATCH_FIELDS] = {
		a->flags.has_resource && b->flags.has_resource,
		a->flags.has_class && b->flags.has_class,
		a->flags.has_name && b->flags.has_name,
		a->flags.has_icon && b->flags.has_icon
	};

	if (a->flags.has_window_id && b->flags.has_window_id &&
	    a->window_id != b->window_id)
		return false;
	for (unsigned i = 0; i < STYLE_MATCH_FIELDS; i++)
	{
		if (shared[i] && !pattern_spans_may_overlap(
		    av[i], ap, bv[i], bp, i))
			return false;
	}
	return true;
}
