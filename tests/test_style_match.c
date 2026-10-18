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
		assert(calls == 1 && length_calls == 0);
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
				bool may_overlap;

				b.name = b.resource = b.class = b.icon =
					patterns[j];
				style_match_prepare(&bp, &b);
				may_overlap = style_match_ids_may_overlap(&a,
					&ap, &b, &bp);

				assert(may_overlap ==
					style_match_ids_may_overlap(&b, &bp, &a, &ap));
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
		{ "*-alpha", "*-beta" }, { "a", "aa" }
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
	unnecessary_length_scans();
	overlap_policy();
	return 0;
}
