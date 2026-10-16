/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fvwm/fvwm.h"
#include "fvwm/style_match.h"
#include "libs/wild.h"

static unsigned calls;

int __real_matchWildcards(const char *, const char *);
int __wrap_matchWildcards(const char *pattern, const char *value)
{
	calls++;
	return __real_matchWildcards(pattern, value);
}

/* Preserve the original predicate as the differential oracle. */
static bool reference(const style_id_t *id, const style_match_subject *subject)
{
	if (id->flags.is_compatibility_mode &&
	    !(matchWildcards(id->name, subject->value[0]) ||
	      matchWildcards(id->name, subject->value[1]) ||
	      matchWildcards(id->name, subject->value[2])))
		return false;
	if (id->flags.has_resource &&
	    !matchWildcards(id->resource, subject->value[0]))
		return false;
	if (id->flags.has_class &&
	    !matchWildcards(id->class, subject->value[1]))
		return false;
	if (id->flags.has_name &&
	    !matchWildcards(id->name, subject->value[2]))
		return false;
	if (id->flags.has_icon &&
	    !matchWildcards(id->icon, subject->value[3]))
		return false;
	if (id->flags.has_window_id && id->window_id != subject->window)
		return false;
	return true;
}

static unsigned random_value(void)
{
	static unsigned state = 0x7319b5U;

	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

static void set_flags(style_id_t *id, unsigned fields)
{
	id->flags.has_resource = !!(fields & 1);
	id->flags.has_class = !!(fields & 2);
	id->flags.has_name = !!(fields & 4);
	id->flags.has_icon = !!(fields & 8);
	id->flags.has_window_id = !!(fields & 16);
	id->flags.is_compatibility_mode = !!(fields & 32);
}

static void compare(const style_id_t *id, style_match_subject *subject)
{
	style_match_patterns patterns;

	style_match_prepare(&patterns, id);
	assert(style_match_id(id, &patterns, subject) == reference(id, subject));
}

static void differential(void)
{
	const char alphabet[] = "ab*?\\";
	const char *values[] = { NULL, "", "a", "b", "ab", "aa", "ba", "*",
		"?", "\\", "aba", "other", "\303\251", "\377" };
	style_id_t id = { 0 };
	style_match_subject subject;

	/* Enumerate all short patterns, including malformed trailing escapes,
	 * and every active-selector combination. */
	for (unsigned length = 0, count = 1; length <= 4; length++, count *= 5)
	{
		for (unsigned word = 0; word < count; word++)
		{
			char pattern[5];
			unsigned digits = word;

			for (unsigned j = 0; j < length; j++, digits /= 5)
				pattern[j] = alphabet[digits % 5];
			pattern[length] = '\0';
			id.name = id.class = id.resource = id.icon = pattern;
			for (unsigned flags = 0; flags < 64; flags++)
			{
				set_flags(&id, flags);
				for (unsigned v = 0; v < sizeof(values) / sizeof(*values); v++)
				{
					id.window_id = v & 1 ? 19 : 23;
					style_match_subject_init(&subject, 19, values[v],
						values[(v + 3) % 14], values[v], values[(v + 7) % 14]);
					compare(&id, &subject);
				}
			}
		}
	}
	/* Independently varied fields catch conjunction/field-index mistakes.
	 * A fixed seed makes every failure reproducible. */
	for (unsigned i = 0; i < 100000; i++)
	{
		char pattern[4][9];
		char **fields[] = { &id.resource, &id.class, &id.name, &id.icon };

		set_flags(&id, random_value() & 63);
		id.window_id = random_value() & 3;
		for (unsigned j = 0; j < 4; j++)
		{
			unsigned length = random_value() % 9;

			for (unsigned k = 0; k < length; k++)
				pattern[j][k] = alphabet[random_value() % 5];
			pattern[j][length] = '\0';
			*fields[j] = random_value() % 16 ? pattern[j] : NULL;
		}
		style_match_subject_init(&subject, random_value() & 3,
			values[random_value() % 14], values[random_value() % 14],
			values[random_value() % 14], values[random_value() % 14]);
		compare(&id, &subject);
	}
	puts("legacy wildcard/NULL/escape and all selector combinations: passed");
}

static void work_counts(void)
{
	style_id_t id = { 0 };
	style_match_patterns patterns;
	style_match_subject subject;
	char resource[] = "equal", class[] = "equal", name[] = "equal";

	id.flags.is_compatibility_mode = 1;
	id.name = "equal";
	style_match_subject_init(&subject, 19, resource, class, name, "icon");
	assert(subject.compatibility_fields == 1);
	style_match_prepare(&patterns, &id);
	calls = 0;
	assert(style_match_id(&id, &patterns, &subject) && calls == 0);
	id.name = "*";
	style_match_prepare(&patterns, &id);
	assert(style_match_id(&id, &patterns, &subject) && calls == 0);
	puts("literal and universal selectors bypass wildcard calls: passed");

	id.name = "different*";
	style_match_prepare(&patterns, &id);
	assert(!style_match_id(&id, &patterns, &subject) && calls == 0);
	id.name = "?different*";
	style_match_prepare(&patterns, &id);
	assert(!style_match_id(&id, &patterns, &subject) && calls == 0);
	puts("prefix rejection and equal compatibility fields: passed");

	id.flags.has_window_id = 1;
	id.window_id = 23;
	calls = 0;
	assert(!style_match_id(&id, &patterns, &subject) && calls == 0);
	puts("mismatched WindowID rejects before wildcard calls: passed");

	/* Metadata is rebuilt when selectors are replaced; the subject is
	 * rebuilt when the window's identity changes. Neither owns storage. */
	id.flags.has_window_id = 0;
	id.name = "e*";
	style_match_prepare(&patterns, &id);
	assert(style_match_id(&id, &patterns, &subject));
	strcpy(resource, "other");
	strcpy(class, "other");
	strcpy(name, "other");
	style_match_subject_init(&subject, 19, resource, class, name, NULL);
	assert(!style_match_id(&id, &patterns, &subject));
	puts("fresh identity and selector metadata after replacement: passed");
}

static void wildcard_work_counts(void)
{
	style_id_t id = { 0 };
	style_match_patterns patterns, previous;
	style_match_subject subject;
	const char *selectors[] = {
		"probe-????????*", "probe-?*expected", "probe-*tail"
	};
	const char *values[] = {
		"probe-x", "probe-long-enough-wrong", "probe-long-tail"
	};

	id.flags.has_name = 1;
	for (unsigned i = 0; i < 3; i++)
	{
		id.name = (char *)selectors[i];
		style_match_prepare(&patterns, &id);
		style_match_subject_init(&subject, 1, NULL, NULL, values[i], NULL);
		previous = patterns;
		previous.unescaped = previous.single_star = 0;
		calls = 0;
		assert(style_match_id(&id, &previous, &subject) == (i == 2));
		assert(calls == 1);
		calls = 0;
		assert(style_match_id(&id, &patterns, &subject) == (i == 2));
		assert(calls == 0);
		assert(subject.length_fields == (1U << STYLE_MATCH_NAME));
	}
	/* Non-overlap is required, even when both literal spans match. */
	id.name = "aba*aba";
	style_match_prepare(&patterns, &id);
	style_match_subject_init(&subject, 1, NULL, NULL, "ababa", NULL);
	assert(!style_match_id(&id, &patterns, &subject));
	/* Multi-star empty-string and escaped wildcard quirks stay with the
	 * original owner. Test both positive and negative fallbacks. */
	const char *fallbacks[] = { "**", "*?*", "a**b", "a\\*b", "a\\" };
	const char *subjects[] = { "", "abc", "ab", "a*b", "a" };

	for (unsigned i = 0; i < 5; i++)
	{
		id.name = (char *)fallbacks[i];
		style_match_prepare(&patterns, &id);
		style_match_subject_init(&subject, 1, NULL, NULL, subjects[i], NULL);
		calls = 0;
		bool result = style_match_id(&id, &patterns, &subject);

		assert(calls == 1);
		assert(result == reference(&id, &subject));
	}
	printf("wildcard minimum/suffix/single-star calls: 1 -> 0 each; "
		"metadata %zu bytes, subject %zu bytes: passed\n",
		sizeof(patterns), sizeof(subject));
}

int main(void)
{
	differential();
	work_counts();
	wildcard_work_counts();
	return 0;
}
