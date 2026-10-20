/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "style_merge_probe.h"
#include "fvwm/functions.h"

enum { MERGE_RULES = 160 };
static const style_id_t *visits[2][MERGE_RULES];
static unsigned counts[2], buckets[2], capture;

void style_merge_visit(const style_id_t *id, unsigned bucket)
{
	if (!capture || !id->flags.has_resource || !id->resource ||
	    strncmp(id->resource, "front-R", 7) != 0)
		return;
	unsigned slot = capture - 1;

	assert(counts[slot] < MERGE_RULES);
	visits[slot][counts[slot]++] = id;
	if (bucket < 9)
		buckets[slot] |= 1U << bucket;
}

static void command(const exec_context_t *exc, const char *text)
{
	execute_function_override_window(NULL, exc, (char *)text, NULL,
		FUNC_DONT_DEFER, NULL);
}

/* All four explicit flags share one priority list. Each of four literal
 * necessary-key domains and fallback owns 32 distinct, matching rules. */
static void selector(char *text, size_t capacity, unsigned rule)
{
	char stars[33];
	unsigned domain = rule % 5, length = rule / 5 + 1;
	char resource[64], class[64], name[64], icon[64];

	assert(rule < MERGE_RULES && length < sizeof(stars));
	memset(stars, '*', length);
	stars[length] = '\0';
	/* Legacy repeated stars need nonempty text after the verified prefix.
	 * A literal tail makes all generated selectors match the sample. */
	snprintf(resource, sizeof(resource), "front-R%stail",
		domain == 0 ? "" : domain >= 3 ? stars : "*");
	snprintf(class, sizeof(class), "front-C%stail",
		domain == 1 ? "" : domain == 0 ? stars : "*");
	snprintf(name, sizeof(name), "front-N%stail",
		domain == 2 ? "" : domain == 1 ? stars : "*");
	snprintf(icon, sizeof(icon), "front-I%stail",
		domain == 3 ? "" : domain == 2 ? stars : "*");
	int written = snprintf(text, capacity,
		"(Resource %s, Class %s, Name %s, Icon %s)",
		resource, class, name, icon);

	assert(written > 0 && (size_t)written < capacity);
}

static unsigned serial(unsigned position, unsigned shape)
{
	return shape == 0 ? (position % 32) * 5 + position / 32 : position;
}

static void compare(FvwmWindow *sample, unsigned alive, unsigned last,
	void (*lookup)(FvwmWindow *, window_style *, Bool))
{
	window_style reference, candidate;
	char title[64];

	counts[0] = counts[1] = 0;
	buckets[0] = buckets[1] = 0;
	/* Both traversals borrow the same live rules. No command runs until
	 * after the independent linear visit sequence has been compared. */
	capture = 1;
	lookup(sample, &reference, True);
	capture = 2;
	lookup(sample, &candidate, False);
	capture = 0;
	assert(counts[0] == alive && counts[1] == alive);
	for (unsigned i = 0; i < alive; i++)
		assert(visits[0][i] == visits[1][i]);
	assert(memcmp(&reference, &candidate, sizeof(candidate)) == 0);
	printf("STYLE_MERGE_CHECK {\"alive\":%u,\"last\":%u,"
		"\"width\":%d,\"title\":\"%s\"}\n", alive, last,
		SGET_BORDER_WIDTH(candidate),
		SGET_TITLE_FORMAT_STRING(candidate));
	fflush(stdout);
	assert(SGET_BORDER_WIDTH(candidate) == 3 + last % 10);
	snprintf(title, sizeof(title), "front-%u %%n", last);
	assert(strcmp(SGET_TITLE_FORMAT_STRING(candidate), title) == 0);
}

static uint64_t now_ns(void)
{
	struct timespec time;

	assert(clock_gettime(CLOCK_MONOTONIC, &time) == 0);
	return (uint64_t)time.tv_sec * UINT64_C(1000000000) + time.tv_nsec;
}

void style_merge_probe(const exec_context_t *exc, FvwmWindow *window,
	void (*lookup)(FvwmWindow *, window_style *, Bool),
	void (*distribution)(unsigned))
{
	FvwmWindow sample = *window;
	char id[320], text[512];
	unsigned cases = 0;

	sample.class.res_name = "front-Rtail";
	sample.class.res_class = "front-Ctail";
	sample.name.name = "front-Ntail";
	sample.icon_name.name = "front-Itail";
	for (unsigned shape = 0; shape < 4; shape++)
	{
		Bool alive[MERGE_RULES];
		unsigned last = serial(MERGE_RULES - 1, shape);

		distribution(shape < 2 ? 0 : shape - 1);
		for (unsigned position = 0; position < MERGE_RULES; position++)
		{
			unsigned rule = serial(position, shape);

			alive[rule] = True;
			selector(id, sizeof(id), rule);
			snprintf(text, sizeof(text),
					"Style %s BorderWidth %u, "
					"TitleFormat front-%u %%n",
				id, 3 + rule % 10, rule);
			command(exc, text);
		}
		compare(&sample, MERGE_RULES, last, lookup);
		/* The synthetic partition covers every bounded slot.
			 * Only test-derived buckets change; rule IDs remain owned. */
		if (shape == 3)
			assert(buckets[0] == 511 && buckets[1] == 511);
		cases++;
		if (getenv("FVWM_STYLE_MERGE_BENCH"))
		{
			window_style result;
			uint64_t start = now_ns();

			for (unsigned i = 0; i < 512; i++)
				lookup(&sample, &result, False);
			printf("STYLE_MERGE_BENCH {\"shape\":%u,"
				"\"lookups\":512,\"nanoseconds\":%llu}\n",
				shape, (unsigned long long)(now_ns() - start));
		}
		/* Retire head/middle/tail rules, including an exhausted owner,
		 * then append one again after gaps in the original ordinals. */
		for (unsigned position = 0; position < MERGE_RULES; position++)
		{
			unsigned rule = serial(position, shape);

			if (rule % 5 != 2 && position != 0 &&
			    position != MERGE_RULES / 2 &&
			    position != MERGE_RULES - 1)
				continue;
			selector(id, sizeof(id), rule);
			snprintf(text, sizeof(text), "DestroyStyle %s", id);
			command(exc, text);
			alive[rule] = False;
		}
		unsigned remaining = 0;

		for (unsigned position = 0; position < MERGE_RULES; position++)
		{
			unsigned rule = serial(position, shape);

			if (alive[rule])
			{
				remaining++;
				last = rule;
			}
		}
		compare(&sample, remaining, last, lookup);
		cases++;
		selector(id, sizeof(id), 2);
		snprintf(text, sizeof(text),
			"Style %s BorderWidth 5, TitleFormat front-2 %%n", id);
		command(exc, text);
		alive[2] = True;
		compare(&sample, remaining + 1, 2, lookup);
		cases++;
		for (unsigned rule = 0; rule < MERGE_RULES; rule++)
		{
			if (!alive[rule])
				continue;
			selector(id, sizeof(id), rule);
			snprintf(text, sizeof(text), "DestroyStyle %s", id);
			command(exc, text);
		}
		window_style empty;

		lookup(&sample, &empty, False);
		assert(SGET_BORDER_WIDTH(empty) == 3);
		distribution(0);
	}
	printf("STYLE_MERGE_WORK {\"cases\":%u,\"rules\":160,"
		"\"shapes\":4,\"linear_reference\":true}\n", cases);
}
