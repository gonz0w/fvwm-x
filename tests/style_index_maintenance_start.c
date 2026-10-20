/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/initial_window_options.h"
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
static unsigned allocations;

void *__real_fxcalloc(size_t, size_t);
void *__wrap_fxcalloc(size_t count, size_t size)
{
	allocations += measuring;
	return __real_fxcalloc(count, size);
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
	if (!collision)
		return __real_style_match_lookup_subject(subject, domains, buckets);
	buckets[0] = 0;
	buckets[1] = 1;
	return 2;
}

static void command(const exec_context_t *exc, const char *text)
{
	execute_function_override_window(NULL, exc, (char *)text, NULL,
		FUNC_DONT_DEFER, NULL);
}

static window_style compare(FvwmWindow *sample, Bool observed, Bool baseline)
{
	window_style indexed, linear;

	allocations = 0;
	measuring = observed;
	lookup_style(sample, &indexed);
	measuring = False;
	if (observed)
		assert(allocations == (baseline ? 1U : 0U));
	reference = True;
	lookup_style(sample, &linear);
	reference = False;
	assert(memcmp(&indexed, &linear, sizeof(indexed)) == 0);
	return indexed;
}

static void check_border(FvwmWindow *window, unsigned width)
{
	Atom type;
	int format;
	unsigned long count, remaining;
	unsigned char *data = NULL;
	XWindowAttributes frame, client;

	assert(window->boundary_width == width);
	assert(XGetWindowProperty(dpy, FW_W(window),
		XInternAtom(dpy, "_NET_FRAME_EXTENTS", False), 0, 4, False,
		XA_CARDINAL, &type, &format, &count, &remaining, &data) ==
		Success);
	assert(type == XA_CARDINAL && format == 32 && count == 4);
	assert(remaining == 0 && data != NULL);
	for (unsigned i = 0; i < 4; i++)
	{
		assert(((unsigned long *)data)[i] == width);
	}
	XFree(data);
	assert(XGetWindowAttributes(dpy, FW_W_FRAME(window), &frame));
	assert(XGetWindowAttributes(dpy, FW_W(window), &client));
	assert(frame.width == client.width + 2 * (int)width);
	assert(frame.height == client.height + 2 * (int)width);
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


void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	Bool baseline = getenv("FVWM_STYLE_INDEX_BASELINE") != NULL;
	FvwmWindow *window;
	window_style style;
	char text[256], name[64];

	alarm(45);
	fFvwmInStartup = False;
	command(exc, "Style * NoTitle, BorderWidth 3, HandleWidth 3");
	command(exc, "UpdateStyles");
	window = create_window(exc);
	for (unsigned mode = 0; mode < 2; mode++)
	{
		FvwmWindow sample = *window;

		collision = mode;
		for (unsigned i = 0; i < 64; i++)
		{
			snprintf(text, sizeof(text),
				"Style (Name maintain-%u) BorderWidth %u", i,
				5 + i % 3);
			command(exc, text);
		}
		sample.name.name = "maintain-0";
		compare(&sample, False, baseline);
		for (unsigned i = 64; i < 80; i++)
		{
			snprintf(name, sizeof(name), "maintain-%u", i);
			snprintf(text, sizeof(text),
				"Style (Name %s) BorderWidth 9", name);
			command(exc, text);
			sample.name.name = name;
			style = compare(&sample, True, baseline);
			assert(SGET_BORDER_WIDTH(style) == 9);
		}
		printf("index-append collision=%u rebuilds=%u: passed\n",
			mode, baseline ? 16 : 0);
		for (unsigned i = 64; i < 80; i++)
		{
			snprintf(text, sizeof(text),
				"DestroyStyle (Name maintain-%u)", i);
			command(exc, text);
			snprintf(name, sizeof(name), "maintain-%u", i);
			sample.name.name = name;
			style = compare(&sample, True, baseline);
			assert(SGET_BORDER_WIDTH(style) == 3);
		}
		printf("index-delete collision=%u rebuilds=%u: passed\n",
			mode, baseline ? 16 : 0);
		for (unsigned kind = 0; kind < 3; kind++)
		{
			command(exc, "Style (Name maintain-repeat) BorderWidth 5");
			command(exc, kind == 2 ?
				"Style (Name maintain-*) BorderWidth 11" :
				"Style (Name maintain-gap) BorderWidth 11");
			command(exc, kind == 0 ?
				"Style (Name maintain-repeat) BorderWidth 7" :
				"Style (Name maintain-repeat) HandleWidth 7");
			sample.name.name = "maintain-repeat";
			compare(&sample, False, baseline);
			simplify_style_list();
			style = compare(&sample, True, baseline);
			assert(SGET_BORDER_WIDTH(style) ==
				(kind == 0 ? 7 : kind == 1 ? 5 : 11));
			assert(SGET_HANDLE_WIDTH(style) == (kind ? 7 : 3));
			printf("index-simplify-%u collision=%u rebuilds=%u: "
				"passed\n", kind, mode, baseline ? 1 : 0);
			command(exc, "DestroyStyle (Name maintain-repeat)");
			command(exc, "DestroyStyle (Name maintain-gap)");
			command(exc, "DestroyStyle (Name maintain-*)");
			compare(&sample, False, baseline);
		}
		/* Apply from the maintained index to actual X geometry, then
		 * retire every rule and cross back through the linear threshold. */
		command(exc, "Style (Name 'update barrier control') "
			"BorderWidth 8, HandleWidth 8");
		command(exc, "UpdateStyles");
		check_border(window, 8);
		command(exc, "DestroyStyle (Name 'update barrier control')");
		command(exc, "UpdateStyles");
		check_border(window, 3);
		for (unsigned i = 0; i < 64; i++)
		{
			snprintf(text, sizeof(text),
				"DestroyStyle (Name maintain-%u)", i);
			command(exc, text);
			sample.name.name = "maintain-0";
			compare(&sample, False, baseline);
		}
		command(exc, "DestroyStyle (Name maintain-repeat)");
		command(exc, "DestroyStyle (Name maintain-gap)");
		simplify_style_list();
		command(exc, "UpdateStyles");
	}
	puts("style index maintenance: passed");
	exit(0);
}
