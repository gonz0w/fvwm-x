/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include "libs/wcontext.h"
#include "fvwm/fvwm.h"
#include "fvwm/add_window.h"
#include "fvwm/eventhandler.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/functions.h"
#include "fvwm/screen.h"
#include "fvwm/update.h"

#include <stdlib.h>
#include <string.h>
#include "fvwm/style.h"
#include "fvwm/builtins.h"
#include "fvwm/colorset.h"
#include "fvwm/focus.h"

#include "fvwm/commands.h"
#include "fvwm/style_match.h"

static Bool measuring, reference, collision;
static unsigned allocations, domains_seen, fields_hashed, index_frees;
static void *index_pointer;

void *__real_fxcalloc(size_t, size_t);
void *__wrap_fxcalloc(size_t count, size_t size)
{
	void *pointer = __real_fxcalloc(count, size);

	if (measuring)
	{
		allocations++;
		index_pointer = pointer;
		printf("index allocation bytes=%zu\n", count * size);
	}
	return pointer;
}

void __real_free(void *);
void __wrap_free(void *pointer)
{
	if (pointer && pointer == index_pointer)
	{
		index_pointer = NULL;
		index_frees++;
	}
	__real_free(pointer);
}

bool __real_style_match_lookup_enabled(size_t, size_t);
bool __wrap_style_match_lookup_enabled(size_t count, size_t indexed)
{
	return !reference && __real_style_match_lookup_enabled(count, indexed);
}

void __real_style_match_prepare(style_match_patterns *, const style_id_t *);
void __wrap_style_match_prepare(style_match_patterns *patterns,
	const style_id_t *id)
{
	__real_style_match_prepare(patterns, id);
	if (collision && patterns->lookup_bucket)
		patterns->lookup_bucket = 1;
}

unsigned __real_style_match_lookup_subject(style_match_subject *, unsigned,
	unsigned [STYLE_LOOKUP_STREAMS]);
unsigned __wrap_style_match_lookup_subject(style_match_subject *subject,
	unsigned domains, unsigned buckets[STYLE_LOOKUP_STREAMS])
{
	unsigned count = __real_style_match_lookup_subject(
		subject, domains, buckets);

	if (measuring)
	{
		domains_seen |= domains;
		fields_hashed |= subject->hash_fields;
	}
	if (!collision)
		return count;
	buckets[0] = 0;
	buckets[1] = 1;
	return 2;
}

static void command(const exec_context_t *exc, const char *text)
{
	execute_function_override_window(NULL, exc, (char *)text, NULL,
		FUNC_DONT_DEFER, NULL);
}

static window_style compare(FvwmWindow *sample, unsigned expected_allocations,
	unsigned expected_domains)
{
	window_style indexed, linear;

	allocations = domains_seen = fields_hashed = 0;
	measuring = True;
	lookup_style(sample, &indexed);
	measuring = False;
	assert(allocations == expected_allocations);
	assert(domains_seen == expected_domains);
	assert(fields_hashed == expected_domains);
	reference = True;
	lookup_style(sample, &linear);
	reference = False;
	assert(memcmp(&indexed, &linear, sizeof(indexed)) == 0);
	return indexed;
}

static FvwmWindow *create_window(const exec_context_t *exc)
{
	XSizeHints hints = { 0 };
	exec_context_changes_t changes = { 0 };
	initial_window_options_t options = { 0 };
	const char *map_command = NULL;
	const exec_context_t *context;
	FvwmWindow *window;
	Window client = XCreateSimpleWindow(dpy, Scr.Root,
		10, 10, 100, 80, 0, 0, 0);

	XStoreName(dpy, client, "update barrier control");
	hints.flags = USPosition | USSize;
	hints.x = hints.y = 10;
	hints.width = 100;
	hints.height = 80;
	XSetWMNormalHints(dpy, client, &hints);
	XSync(dpy, False);
	changes.w.w = client;
	changes.w.wcontext = C_ROOT;
	context = exc_clone_context(exc, &changes,
		ECC_W | ECC_FW | ECC_WCONTEXT);
	options.initial_state = NormalState;
	window = AddWindow(&map_command, context, NULL, &options);
	assert(window != NULL && FW_W(window) == client);
	exc_destroy_context(context);
	return window;
}


static void selector(char *text, size_t size, const char *verb,
	unsigned field, unsigned number)
{
	const char *values[] = { "*", "*", "*", "*" };
	char literal[64];

	snprintf(literal, sizeof(literal), "retention-%u-%u", field, number);
	values[field] = literal;
	snprintf(text, size, "%s (Resource %s, Class %s, Name %s, Icon %s)%s",
		verb, values[0], values[1], values[2], values[3],
		strcmp(verb, "Style") == 0 ? " BorderWidth 9" : "");
}

static void check_categories(const exec_context_t *exc, FvwmWindow *window)
{
	const char *ids[] = { "(*)", "retention", "(Resource retention)",
		"(Class retention)", "(Name retention)", "(Icon retention)" };
	FvwmWindow sample = *window;
	window_style style;
	char text[256];

	sample.class.res_name = sample.class.res_class = "retention";
	sample.name.name = sample.icon_name.name = "retention";
	snprintf(text, sizeof(text), "Style (WindowID 0x%lx) BorderWidth 9",
		FW_W(window));
	command(exc, text);
	/* Insert in reverse category order so command order cannot hide a
	 * category precedence change. */
	for (int i = 5; i >= 0; i--)
	{
		snprintf(text, sizeof(text), "Style %s BorderWidth %d", ids[i],
			i + 3);
		command(exc, text);
	}
	style = compare(&sample, 0, 0);
	assert(SGET_BORDER_WIDTH(style) == 9);
	snprintf(text, sizeof(text), "DestroyStyle (WindowID 0x%lx)",
		FW_W(window));
	command(exc, text);
	for (int i = 5; i >= 0; i--)
	{
		style = compare(&sample, 0, 0);
		assert(SGET_BORDER_WIDTH(style) == i + 3);
		snprintf(text, sizeof(text), "DestroyStyle %s", ids[i]);
		command(exc, text);
	}
	puts("seven category precedence: passed");
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	Bool baseline = getenv("FVWM_STYLE_RETENTION_BASELINE") != NULL;
	FvwmWindow *window;
	char text[256], long_value[16385];

	alarm(45);
	fFvwmInStartup = False;
	command(exc, "Style * NoTitle, BorderWidth 3, HandleWidth 3");
	command(exc, "UpdateStyles");
	window = create_window(exc);
	check_categories(exc, window);
	memset(long_value, 'x', sizeof(long_value) - 1);
	long_value[sizeof(long_value) - 1] = 0;
	for (unsigned mode = 0; mode < 2; mode++)
	{
		FvwmWindow sample = *window;
		unsigned expected = 15;

		collision = mode;
		sample.class.res_name = sample.class.res_class = long_value;
		sample.name.name = sample.icon_name.name = long_value;
		for (unsigned field = 0; field < 4; field++)
		{
			for (unsigned i = 0; i < (field == 3 ? 64U : 2U); i++)
			{
				selector(text, sizeof(text), "Style", field, i);
				command(exc, text);
			}
		}
		/* The baseline keeps the first mode's empty table. */
		compare(&sample, baseline && mode ? 0 : 1, expected);
		index_frees = 0;
		for (unsigned field = 0; field < 3; field++)
		{
			selector(text, sizeof(text), "DestroyStyle", field, 0);
			command(exc, text);
			compare(&sample, 0, expected);
			selector(text, sizeof(text), "DestroyStyle", field, 1);
			command(exc, text);
			if (!baseline)
				expected &= ~(1U << field);
			compare(&sample, 0, expected);
			printf("retired-domain collision=%u field=%u domains=%u: "
				"passed\n", mode, field, expected);
		}
		/* Reintroduce a retired domain, then retire it again. */
		selector(text, sizeof(text), "Style", 0, 2);
		command(exc, text);
		compare(&sample, 0, expected | 1);
		selector(text, sizeof(text), "DestroyStyle", 0, 2);
		command(exc, text);
		compare(&sample, 0, expected);
		for (unsigned i = 0; i < 64; i++)
		{
			selector(text, sizeof(text), "DestroyStyle", 3, i);
			command(exc, text);
			assert(index_frees == (!baseline && i == 63 ? 1U : 0U));
		}
		assert(baseline || index_pointer == NULL);
		compare(&sample, 0, 0);
		printf("empty-index collision=%u frees=%u: passed\n",
			mode, index_frees);
	}
	puts("style index retention: passed");
	exit(0);
}
