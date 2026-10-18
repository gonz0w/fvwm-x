/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fvwm/fvwm.h"
#include "fvwm/style_match.h"
#include "libs/wild.h"

static unsigned calls, length_calls, equality_calls, call_depth;

int __real_strcmp(const char *, const char *);
int __wrap_strcmp(const char *a, const char *b)
{
	equality_calls++;
	return __real_strcmp(a, b);
}

size_t __real_strlen(const char *);
size_t __wrap_strlen(const char *value)
{
	length_calls++;
	return __real_strlen(value);
}

int __real_matchWildcards(const char *, const char *);
int __wrap_matchWildcards(const char *pattern, const char *value)
{
	int result;

	/* Some compiler/linker choices wrap wild.c's own recursion too.
	 * Count caller invocations independently of that implementation. */
	if (call_depth++ == 0)
		calls++;
	result = __real_matchWildcards(pattern, value);
	call_depth--;
	return result;
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
	bool matches = reference(id, subject);
	unsigned buckets[STYLE_LOOKUP_STREAMS];
	unsigned count = style_match_lookup_subject(subject,
		patterns.lookup_domain, buckets);
	bool candidate = false;

	assert(count > 0 && count <= STYLE_LOOKUP_STREAMS);
	assert(patterns.lookup_bucket < STYLE_LOOKUP_BUCKETS);
	for (unsigned i = 0; i < count; i++)
	{
		assert(buckets[i] < STYLE_LOOKUP_BUCKETS);
		for (unsigned j = 0; j < i; j++)
			assert(buckets[i] != buckets[j]);
		candidate |= buckets[i] == patterns.lookup_bucket;
	}
	assert(!matches || candidate);
	assert(style_match_id(id, &patterns, subject) == matches);
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
	assert(subject.compatibility_fields == 0);
	style_match_prepare(&patterns, &id);
	calls = 0;
	assert(style_match_id(&id, &patterns, &subject) && calls == 0);
	assert(subject.compatibility_fields == 1);
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
		style_match_subject_init(&subject, 1, NULL, NULL,
			values[i], NULL);
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
	/* Preserve empty tails, escaped literals and trailing escapes. */
	const char *fallbacks[] = { "**", "*?*", "a**b", "a\\*b", "a\\" };
	const char *subjects[] = { "", "abc", "ab", "a*b", "a" };

	for (unsigned i = 0; i < 5; i++)
	{
		id.name = (char *)fallbacks[i];
		style_match_prepare(&patterns, &id);
		style_match_subject_init(&subject, 1, NULL, NULL, subjects[i], NULL);
		calls = 0;
		bool result = style_match_id(&id, &patterns, &subject);

		assert(calls == 0);
		assert(result == reference(&id, &subject));
	}
	printf("wildcard minimum/suffix/single-star calls: 1 -> 0 each; "
		"metadata %zu bytes, subject %zu bytes: passed\n",
		sizeof(patterns), sizeof(subject));
}

static void fixed_width_patterns(void)
{
	style_id_t id = { 0 };
	style_match_patterns patterns;
	style_match_subject subject;
	char pattern[263], value[264];

	id.flags.has_name = 1;
	id.name = pattern;
	strcpy(pattern, "probe-");
	memset(pattern + 6, '?', 256);
	pattern[262] = '\0';
	strcpy(value, "probe-");
	memset(value + 6, 'a', 257);
	value[263] = '\0';
	style_match_prepare(&patterns, &id);
	assert(patterns.fixed_width == (1U << STYLE_MATCH_NAME));
	for (unsigned extra = 0; extra < 2; extra++)
	{
		value[262] = extra ? 'a' : '\0';
		style_match_subject_init(&subject, 1, NULL, NULL, value, NULL);
		bool expected = reference(&id, &subject);

		calls = 0;
		assert(style_match_id(&id, &patterns, &subject) == expected);
		assert(expected == !extra && calls == 0);
	}
	const char *selectors[] = { "a?b?c", "??", "?\377?", "a\\?b" };
	const char *values[] = { "axbyc", "\303\251", "a\377b", "a?b" };

	for (unsigned i = 0; i < 4; i++)
	{
		id.name = (char *)selectors[i];
		style_match_prepare(&patterns, &id);
		style_match_subject_init(&subject, 1, NULL, NULL,
			values[i], NULL);
		calls = 0;
		assert(style_match_id(&id, &patterns, &subject));
		assert(calls == 0);
		assert(!!patterns.fixed_width == (i != 3));
	}
	puts("fixed-width questions reject unequal byte counts: passed");
}

static void single_star_questions(void)
{
	const char *selectors[] = { "*?", "?*", "a?b*cd?e", "?*?",
		"*?\377?", "?*a?" };
	const char *values[] = { "x", "x", "axb-long-cdye", "x",
		"long-a\377b", "aaa" };
	style_id_t id = { 0 };
	style_match_patterns patterns;
	style_match_subject subject;
	char name[16385];

	id.flags.has_name = 1;
	for (unsigned i = 0; i < 6; i++)
	{
		id.name = (char *)selectors[i];
		style_match_prepare(&patterns, &id);
		assert(patterns.single_star_questions ==
			(1U << STYLE_MATCH_NAME));
		style_match_subject_init(&subject, 1, NULL, NULL,
			values[i], NULL);
		bool expected = reference(&id, &subject);

		calls = 0;
		assert(style_match_id(&id, &patterns, &subject) == expected);
		assert(expected == (i != 3) && calls == 0);
	}
	memset(name, 'a', sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
	id.name = "*?a?";
	style_match_prepare(&patterns, &id);
	style_match_subject_init(&subject, 1, NULL, NULL, name, NULL);
	calls = 0;
	assert(style_match_id(&id, &patterns, &subject) && calls == 0);
	name[sizeof(name) - 2] = 'b';
	id.name = "*?b?";
	style_match_prepare(&patterns, &id);
	style_match_subject_init(&subject, 1, NULL, NULL, name, NULL);
	assert(!reference(&id, &subject));
	calls = 0;
	assert(!style_match_id(&id, &patterns, &subject) && calls == 0);
	puts("one-star question spans avoid suffix retries: passed");
}

static void unescaped_cursors(void)
{
	style_id_t id = { 0 };
	style_match_patterns patterns;
	style_match_subject subject;
	const char alphabet[] = "ab*?";
	unsigned long checked = 0;

	id.flags.has_name = 1;
	for (unsigned length = 0, count = 1; length <= 6;
	     length++, count *= 4)
	{
		for (unsigned word = 0; word < count; word++)
		{
			char pattern[7];
			unsigned digits = word;

			for (unsigned i = 0; i < length; i++, digits /= 4)
				pattern[i] = alphabet[digits % 4];
			pattern[length] = '\0';
			id.name = pattern;
			style_match_prepare(&patterns, &id);
			for (unsigned size = 0, values = 1; size <= 5;
			     size++, values *= 2)
			{
				for (unsigned value = 0; value < values;
				     value++)
				{
					char name[6];
					unsigned bits = value;

					for (unsigned i = 0; i < size;
					     i++, bits /= 2)
						name[i] = "ab"[bits % 2];
					name[size] = '\0';
					style_match_subject_init(&subject, 1,
						NULL, NULL, name, NULL);
					bool expected =
						reference(&id, &subject);

					calls = 0;
					assert(style_match_id(&id, &patterns,
						&subject) == expected);
					assert(calls == 0);
					checked++;
				}
			}
		}
	}
	/* A closed-form family checks deep input without asking the recursive
	 * oracle to consume a corresponding call stack. Short forms above use
	 * the unchanged legacy implementation as their independent oracle. */
	char *deep = malloc(65538);

	assert(deep != NULL);
	memset(deep, '*', 65536);
	strcpy(deep + 65536, "?");
	id.name = deep;
	style_match_prepare(&patterns, &id);
	style_match_subject_init(&subject, 1, NULL, NULL, "a", NULL);
	calls = 0;
	assert(style_match_id(&id, &patterns, &subject) && calls == 0);
	style_match_subject_init(&subject, 1, NULL, NULL, "", NULL);
	assert(!style_match_id(&id, &patterns, &subject) && calls == 0);
	deep[65536] = '\0';
	style_match_prepare(&patterns, &id);
	style_match_subject_init(&subject, 1, NULL, NULL, "a", NULL);
	assert(style_match_id(&id, &patterns, &subject));
	style_match_subject_init(&subject, 1, NULL, NULL, "", NULL);
	assert(!style_match_id(&id, &patterns, &subject));
	free(deep);
	printf("%lu unescaped legacy comparisons and deep cursors: passed\n",
		checked);
}

static void escaped_fixed_width(void)
{
	const char *patterns[] = { "a\\?b", "a\\*?", "\\\\?", "\\a\\b", "a\\",
		"\\?\\*?", "\\\377?" };
	const char *values[] = { "a?b", "a*x", "\\x", "ab", "a", "?*x",
		"\377x", "", "other", "a?bx", "\\" };
	style_id_t id = { 0 };
	style_match_patterns prepared, previous;
	style_match_subject subject;

	id.flags.has_name = 1;
	for (unsigned i = 0; i < sizeof(patterns) / sizeof(*patterns); i++)
	{
		id.name = (char *)patterns[i];
		style_match_prepare(&prepared, &id);
		assert(prepared.escaped_fixed_width ==
			(1U << STYLE_MATCH_NAME));
		assert(!prepared.fixed_width && !prepared.unescaped);
		previous = prepared;
		previous.escaped = previous.escaped_fixed_width = 0;
		for (unsigned j = 0; j < sizeof(values) / sizeof(*values); j++)
		{
			style_match_subject_init(&subject, 1, NULL, NULL,
				values[j], NULL);
			bool expected = reference(&id, &subject);

			calls = 0;
			assert(style_match_id(&id, &prepared,
				&subject) == expected);
			assert(calls == 0);
			assert(style_match_id(&id, &previous,
				&subject) == expected);
		}
	}
	puts("escaped fixed widths preserve quoted bytes "
		"and trailing escapes: passed");
}

static void escaped_single_star(void)
{
	const char *patterns[] = { "\\a?*\\?z", "*\\*?", "\\a*", "*\\",
		"\\?*\\?", "\\\\?*?\\\\", "\\a*\\a" };
	const char *values[] = { "ab-long-?z", "ab-long-xz", "*x", "a", "",
		"?x?", "?", "\\a-long-b\\", "aa", "a-long-a" };
	style_id_t id = { 0 };
	style_match_patterns prepared;
	style_match_subject subject;

	id.flags.has_name = 1;
	for (unsigned i = 0; i < sizeof(patterns) / sizeof(*patterns); i++)
	{
		id.name = (char *)patterns[i];
		style_match_prepare(&prepared, &id);
		assert(prepared.escaped_single_star ==
			(1U << STYLE_MATCH_NAME));
		assert(!prepared.single_star &&
			!prepared.single_star_questions);
		for (unsigned j = 0; j < sizeof(values) / sizeof(*values); j++)
		{
			style_match_subject_init(&subject, 1, NULL, NULL,
				values[j], NULL);
			bool expected = reference(&id, &subject);

			calls = 0;
			assert(style_match_id(&id, &prepared,
				&subject) == expected);
			assert(calls == 0);
		}
	}
	puts("escaped one-star spans preserve quoted wildcards "
		"and non-overlap: passed");
}

static void escaped_cursors(void)
{
	const char alphabet[] = "ab*?\\";
	style_id_t id = { 0 };
	style_match_patterns prepared;
	style_match_subject subject;
	unsigned long checked = 0;

	id.flags.has_name = 1;
	for (unsigned length = 1, count = 5; length <= 5;
	     length++, count *= 5)
	{
		for (unsigned word = 0; word < count; word++)
		{
			char pattern[6];
			unsigned digits = word;

			for (unsigned i = 0; i < length; i++, digits /= 5)
				pattern[i] = alphabet[digits % 5];
			pattern[length] = '\0';
			if (!strchr(pattern, '\\'))
				continue;
			id.name = pattern;
			style_match_prepare(&prepared, &id);
			for (unsigned size = 0, values = 1; size <= 3;
			     size++, values *= 5)
			{
				for (unsigned value = 0; value < values;
				     value++)
				{
					char name[4];
					unsigned parts = value;

					for (unsigned i = 0; i < size;
					     i++, parts /= 5)
						name[i] = alphabet[parts % 5];
					name[size] = '\0';
					style_match_subject_init(&subject, 1,
						NULL, NULL, name, NULL);
					bool expected =
						reference(&id, &subject);

					calls = 0;
					assert(style_match_id(&id, &prepared,
						&subject) == expected);
					assert(calls == 0);
					checked++;
				}
			}
		}
	}
	/* Closed forms avoid sending the deep inputs to the recursive oracle.
	 * Short forms above retain its independent legacy escape semantics. */
	char *deep = malloc(131077), *name = malloc(65538);

	assert(deep && name);
	for (unsigned i = 0; i < 65536; i++)
	{
		deep[2 * i] = '\\';
		deep[2 * i + 1] = 'a';
		name[i] = 'a';
	}
	deep[131072] = name[65536] = '\0';
	id.name = deep;
	style_match_prepare(&prepared, &id);
	style_match_subject_init(&subject, 1, NULL, NULL, name, NULL);
	calls = 0;
	assert(style_match_id(&id, &prepared, &subject) && calls == 0);
	strcpy(deep, "\\a");
	memset(deep + 2, '*', 65536);
	strcpy(deep + 65538, "?\\*");
	style_match_prepare(&prepared, &id);
	style_match_subject_init(&subject, 1, NULL, NULL, "ab*", NULL);
	assert(style_match_id(&id, &prepared, &subject) && calls == 0);
	style_match_subject_init(&subject, 1, NULL, NULL, "a*", NULL);
	assert(!style_match_id(&id, &prepared, &subject) && calls == 0);
	deep[65538] = '\0';
	style_match_prepare(&prepared, &id);
	style_match_subject_init(&subject, 1, NULL, NULL, "a", NULL);
	assert(!style_match_id(&id, &prepared, &subject) && calls == 0);
	style_match_subject_init(&subject, 1, NULL, NULL, "ab", NULL);
	assert(style_match_id(&id, &prepared, &subject) && calls == 0);
	free(name);
	free(deep);
	printf("%lu escaped legacy comparisons and deep cursors: passed\n",
		checked);
}

static void unnecessary_length_scans(void)
{
	style_id_t id = { 0 };
	style_match_patterns patterns;
	style_match_subject subject;
	char value[16384];

	memset(value, 'x', sizeof(value) - 1);
	memcpy(value, "probe-", 6);
	value[sizeof(value) - 1] = '\0';
	id.flags.has_name = 1;
	id.name = "probe-*";
	style_match_prepare(&patterns, &id);
	style_match_subject_init(&subject, 1, NULL, NULL, value, NULL);
	calls = length_calls = 0;
	assert(style_match_id(&id, &patterns, &subject));
	assert(calls == 0 && length_calls == 0);
	for (unsigned empty = 0; empty < 2; empty++)
	{
		id.name = "**";
		style_match_prepare(&patterns, &id);
		style_match_subject_init(&subject, 1, NULL, NULL,
			empty ? "" : value, NULL);
		calls = length_calls = 0;
		assert(style_match_id(&id, &patterns, &subject) == !empty);
		assert(calls == 0 && length_calls == 0);
	}
	puts("prefix/star-only selectors avoid length scans: passed");
}

static void overlap_policy(void)
{
	const char *values[] = { NULL, "", "a", "b", "aa", "ab", "ba", "bb",
		"aba", "*", "?", "\\", "a*", "b?", "ab-tail", "other" };
	const char alphabet[] = "ab*?\\";
	char patterns[156][4];
	unsigned total = 0, common_matches = 0;
	style_match_patterns ap, bp;
	style_match_subject subject;
	style_id_t a = { 0 }, b = { 0 };

	for (unsigned length = 0, count = 1; length <= 3; length++, count *= 5)
	{
		for (unsigned word = 0; word < count; word++)
		{
			unsigned value = word;

			for (unsigned i = 0; i < length; i++, value /= 5)
				patterns[total][i] = alphabet[value % 5];
			patterns[total++][length] = '\0';
		}
	}
	assert(total == 156);
	for (unsigned field = 0; field < STYLE_MATCH_FIELDS; field++)
	{
		set_flags(&a, 1U << field);
		set_flags(&b, 1U << field);
		for (unsigned i = 0; i < total; i++)
		{
			a.name = a.resource = a.class = a.icon = patterns[i];
			style_match_prepare(&ap, &a);
			for (unsigned j = 0; j < total; j++)
			{
				bool may_overlap, reverse;

				b.name = b.resource = b.class = b.icon =
					patterns[j];
				style_match_prepare(&bp, &b);
				may_overlap = style_match_ids_may_overlap(&a,
					&ap, &b, &bp);

				reverse = style_match_ids_may_overlap(
					&b, &bp, &a, &ap);
				assert(may_overlap == reverse);
				for (unsigned v = 0;
				     v < sizeof(values) / sizeof(*values); v++)
				{
					style_match_subject_init(&subject, 1,
						values[v], values[v],
						values[v], values[v]);
					if (reference(&a, &subject) &&
					    reference(&b, &subject))
					{
						assert(may_overlap);
						common_matches++;
					}
				}
			}
		}
	}
	/* A compatibility selector can match a different identity field from
	 * its neighbour. Explicit constraints on different fields can too. */
	for (unsigned af = 0; af < 64; af++)
	{
		set_flags(&a, af);
		a.name = a.resource = a.class = a.icon = "a";
		a.window_id = 1;
		style_match_prepare(&ap, &a);
		for (unsigned bf = 0; bf < 64; bf++)
		{
			bool may_overlap;

			set_flags(&b, bf);
			b.name = b.resource = b.class = b.icon = "b";
			b.window_id = 1;
			style_match_prepare(&bp, &b);
			may_overlap = style_match_ids_may_overlap(
				&a, &ap, &b, &bp);

			for (unsigned fields = 0; fields < 16; fields++)
			{
				style_match_subject_init(&subject, 1,
					fields & 1 ? "a" : "b",
					fields & 2 ? "a" : "b",
					fields & 4 ? "a" : "b",
					fields & 8 ? "a" : "b");
				if (reference(&a, &subject) &&
				    reference(&b, &subject))
					assert(may_overlap);
			}
		}
	}
	set_flags(&a, 4);
	set_flags(&b, 4);
	const char *disjoint[][2] = {
		{ "alpha", "beta" }, { "alpha*", "beta*" },
		{ "*-alpha", "*-beta" }, { "a", "aa" },
		{ "a??", "a???*" }, { "??", "???" },
		{ "aa", "a??*" }, { "", "?*" },
		{ "?a?", "?b?" }, { "a?b?", "a?c?" },
		{ "ac", "a*b?*" }, { "abc", "a*d*c" },
		{ "", "**" }, { "a", "a**" }
	};

	for (unsigned i = 0; i < sizeof(disjoint) / sizeof(*disjoint); i++)
	{
		a.name = (char *)disjoint[i][0];
		b.name = (char *)disjoint[i][1];
		style_match_prepare(&ap, &a);
		style_match_prepare(&bp, &b);
		assert(!style_match_ids_may_overlap(&a, &ap, &b, &bp));
	}
	printf("selector overlap: %u common-match witnesses retained; "
		"literal/prefix/suffix controls passed\n", common_matches);
}

static void overlap_languages(void)
{
	const char alphabet[] = "ab?*";
	char patterns[85][4], value[7];
	uint64_t witnesses[85][2] = { { 0 } };
	style_id_t a = { 0 }, b = { 0 };
	style_match_patterns ap, bp;
	unsigned total = 0, comparisons = 0;

	for (unsigned length = 0, count = 1; length <= 3; length++, count *= 4)
	{
		for (unsigned word = 0; word < count; word++)
		{
			unsigned bits = word;

			for (unsigned i = 0; i < length; i++, bits /= 4)
				patterns[total][i] = alphabet[bits % 4];
			patterns[total++][length] = '\0';
		}
	}
	assert(total == 85);
	for (unsigned pattern = 0; pattern < total; pattern++)
	{
		unsigned ordinal = 0;

		for (unsigned length = 0; length <= 6; length++)
		{
			for (unsigned word = 0; word < (1U << length); word++)
			{
				for (unsigned i = 0; i < length; i++)
					value[i] = word & (1U << i) ? 'a' : 'b';
				value[length] = '\0';
				if (matchWildcards(patterns[pattern], value))
					witnesses[pattern][ordinal / 64] |=
						UINT64_C(1) << (ordinal % 64);
				ordinal++;
			}
		}
		assert(ordinal == 127);
	}
	for (unsigned field = 0; field < STYLE_MATCH_FIELDS; field++)
	{
		set_flags(&a, 1U << field);
		set_flags(&b, 1U << field);
		for (unsigned i = 0; i < total; i++)
		{
			a.name = a.class = a.resource = a.icon = patterns[i];
			style_match_prepare(&ap, &a);
			for (unsigned j = 0; j < total; j++)
			{
				bool common, may_overlap, reverse;

				b.name = b.class = b.resource = b.icon =
					patterns[j];
				style_match_prepare(&bp, &b);
				common = (witnesses[i][0] & witnesses[j][0]) ||
					(witnesses[i][1] & witnesses[j][1]);
				may_overlap = style_match_ids_may_overlap(
					&a, &ap, &b, &bp);
				assert(!common || may_overlap);
				reverse = style_match_ids_may_overlap(
					&b, &bp, &a, &ap);
				assert(may_overlap == reverse);
				/* Fixed pairs/literals have complete witnesses
				 * in this finite domain. */
				if ((!strchr(patterns[i], '*') &&
				     !strchr(patterns[j], '*')) ||
				    !strpbrk(patterns[i], "?*") ||
				    !strpbrk(patterns[j], "?*"))
					assert(may_overlap == common);
				comparisons++;
			}
		}
	}
	assert(comparisons == 28900);
	puts("overlap languages: 28900 pairs, "
		"127 independent legacy witnesses");
}

static void overlap_proof_budgets(void)
{
	char a_text[4098], b_text[4098], value[4098];
	style_id_t a = { 0 }, b = { 0 };
	style_match_patterns ap, bp;
	style_match_subject subject;

	set_flags(&a, 4);
	set_flags(&b, 4);
	memset(a_text, '?', sizeof(a_text));
	memset(b_text, '?', sizeof(b_text));
	a_text[2048] = 'a';
	b_text[2048] = 'b';
	a_text[4096] = b_text[4096] = '\0';
	a.name = a_text;
	b.name = b_text;
	style_match_prepare(&ap, &a);
	style_match_prepare(&bp, &b);
	assert(!style_match_ids_may_overlap(&a, &ap, &b, &bp));
	/* Beyond the optional scan budget, unknown retains both rules. */
	a_text[4096] = b_text[4096] = '?';
	a_text[4097] = b_text[4097] = '\0';
	style_match_prepare(&ap, &a);
	style_match_prepare(&bp, &b);
	assert(style_match_ids_may_overlap(&a, &ap, &b, &bp));
	memset(value, 'x', 4097);
	value[2048] = 'a';
	value[4097] = '\0';
	style_match_subject_init(&subject, 1, value, value, value, value);
	assert(style_match_id(&a, &ap, &subject) && reference(&a, &subject));
	assert(!style_match_id(&b, &bp, &subject) && !reference(&b, &subject));
	/* Literal membership also stays unknown for a large retry product.
	 * Normal matching accepts the literal and rejects the star rule. */
	value[0] = 'a';
	value[2048] = 'x';
	value[4096] = 'z';
	a.name = value;
	b.name = "a*other*z";
	style_match_prepare(&ap, &a);
	style_match_prepare(&bp, &b);
	assert(style_match_ids_may_overlap(&a, &ap, &b, &bp));
	style_match_subject_init(&subject, 1, value, value, value, value);
	assert(style_match_id(&a, &ap, &subject) && reference(&a, &subject));
	assert(!style_match_id(&b, &bp, &subject) && !reference(&b, &subject));
	puts("bounded overlap proofs retain long-pattern matching: passed");
}

static void deferred_compatibility(void)
{
	style_id_t id = { 0 };
	style_match_patterns patterns;
	style_match_subject subject;
	char a[] = "equal", b[] = "equal", c[] = "equal";

	equality_calls = 0;
	style_match_subject_init(&subject, 19, a, b, c, NULL);
	assert(equality_calls == 0 && subject.compatibility_fields == 0);
	id.flags.has_window_id = 1;
	id.window_id = 19;
	style_match_prepare(&patterns, &id);
	assert(style_match_id(&id, &patterns, &subject));
	assert(equality_calls == 0 && subject.compatibility_fields == 0);
	id.flags.is_compatibility_mode = 1;
	id.name = "equal";
	id.window_id = 23;
	style_match_prepare(&patterns, &id);
	assert(!style_match_id(&id, &patterns, &subject));
	assert(equality_calls == 0 && subject.compatibility_fields == 0);
	id.window_id = 19;
	id.name = "*";
	style_match_prepare(&patterns, &id);
	assert(style_match_id(&id, &patterns, &subject));
	assert(equality_calls == 0 && subject.compatibility_fields == 0);
	id.name = "miss*";
	style_match_prepare(&patterns, &id);
	assert(!style_match_id(&id, &patterns, &subject));
	assert(equality_calls == 2 && subject.compatibility_fields == 1);
	assert(!style_match_id(&id, &patterns, &subject));
	assert(equality_calls == 2);
	style_match_subject_init(&subject, 19, NULL, "", NULL, NULL);
	assert(!style_match_id(&id, &patterns, &subject));
	assert(subject.compatibility_fields == 3);
	puts("compatibility comparisons deferred and reused: passed");
}

static void subject_domains(void)
{
	const char *values[] = { NULL, "", "alpha", "beta", "alpha" };
	style_match_subject subject;
	style_id_t id = { 0 };
	style_match_patterns patterns;
	unsigned buckets[STYLE_LOOKUP_STREAMS];

	/* Every subset may be requested in any list order. Query results
	 * must contain each matching necessary key in the requested domains. */
	for (unsigned tuple = 0; tuple < 625; tuple++)
	{
		unsigned n = tuple;
		const char *v[STYLE_MATCH_FIELDS];

		for (unsigned i = 0; i < STYLE_MATCH_FIELDS; i++, n /= 5)
			v[i] = values[n % 5];
		for (unsigned domains = 0; domains <= STYLE_LOOKUP_ALL;
		     domains++)
		{
			style_match_subject_init(&subject, 19,
				v[0], v[1], v[2], v[3]);
			unsigned count = style_match_lookup_subject(&subject,
				domains, buckets);

			assert(count && count <= STYLE_LOOKUP_STREAMS);
			for (unsigned flags = 0; flags < 64; flags++)
			{
				id.resource = id.class = id.name = id.icon =
					"alpha";
				id.window_id = 19;
				set_flags(&id, flags);
				style_match_prepare(&patterns, &id);
				if (!(patterns.lookup_domain & domains) &&
				    patterns.lookup_bucket)
					continue;
				if (!reference(&id, &subject))
					continue;
				bool found = false;

				for (unsigned i = 0; i < count; i++)
					found |= buckets[i] ==
						patterns.lookup_bucket;
				assert(found);
			}
		}
	}
	style_match_subject_init(&subject, 19, "a", "b", "name", "icon");
	style_match_lookup_subject(&subject, STYLE_LOOKUP_WINDOW, buckets);
	assert(subject.hash_fields == 0 && subject.length_fields == 0);
	assert(subject.compatibility_fields == 0);
	style_match_lookup_subject(&subject, 1U << STYLE_MATCH_NAME, buckets);
	assert(subject.hash_fields == (1U << STYLE_MATCH_NAME));
	assert(subject.length_fields == (1U << STYLE_MATCH_NAME));
	assert(subject.compatibility_fields == 0);
	assert(subject.length[STYLE_MATCH_NAME] == 4);
	id = (style_id_t){ 0 };
	id.flags.has_name = 1;
	id.name = "?*e";
	style_match_prepare(&patterns, &id);
	length_calls = 0;
	assert(style_match_id(&id, &patterns, &subject));
	assert(length_calls == 0);
	style_match_lookup_subject(&subject, STYLE_LOOKUP_ALL, buckets);
	assert(subject.hash_fields == 15 && subject.length_fields == 15);
	for (unsigned i = 0; i < 16; i++)
	{
		style_match_lookup_subject(&subject, i, buckets);
		assert(style_match_id(&id, &patterns, &subject));
	}
	assert(length_calls == 0);
	/* Reinitializing even the same pointers discards all derived data. */
	style_match_subject_init(&subject, 23, NULL, "", NULL, "x");
	assert(subject.hash_fields == 0 && subject.length_fields == 0);
	assert(subject.compatibility_fields == 0 && subject.window_bucket == 0);
	style_match_lookup_subject(&subject, STYLE_LOOKUP_ALL, buckets);
	assert(subject.hash_fields == 10 && subject.length_fields == 10);
	assert(subject.length[STYLE_MATCH_CLASS] == 0);
	assert(subject.length[STYLE_MATCH_ICON] == 1);
	char a[] = "equal", b[] = "equal", c[] = "equal";

	style_match_subject_init(&subject, 19, a, b, c, NULL);
	equality_calls = 0;
	style_match_lookup_subject(&subject, 1U << STYLE_MATCH_FIELDS, buckets);
	assert(equality_calls == 2 && subject.hash_fields == 1);
	style_match_lookup_subject(&subject, 1U << STYLE_MATCH_FIELDS, buckets);
	assert(equality_calls == 2 && subject.hash_fields == 1);
	puts("all domain subsets retain matching keys; hash lengths shared");
}

int main(void)
{
	assert(!style_match_lookup_enabled(31, 31));
	assert(style_match_lookup_enabled(32, 32));
	assert(style_match_lookup_enabled(64, 32));
	assert(!style_match_lookup_enabled(65, 32));
	assert(!style_match_lookup_enabled(SIZE_MAX, 32));
	assert(style_match_lookup_enabled(SIZE_MAX, SIZE_MAX));
	subject_domains();
	deferred_compatibility();
	differential();
	work_counts();
	wildcard_work_counts();
	fixed_width_patterns();
	single_star_questions();
	unescaped_cursors();
	escaped_fixed_width();
	escaped_single_star();
	escaped_cursors();
	unnecessary_length_scans();
	overlap_policy();
	overlap_languages();
	overlap_proof_budgets();
	return 0;
}
