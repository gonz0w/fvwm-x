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
#include "libs/Parse.h"

static Bool tracking, measuring;
static void *owned[32];
static unsigned tokens, live_tokens, grabs;

char *__real_GetNextToken(char *, char **);
char *__wrap_GetNextToken(char *input, char **token)
{
	char *rest = __real_GetNextToken(input, token);

	if (tracking && *token && strncmp(*token, "owned-template-", 15) == 0)
	{
		assert(tokens < sizeof(owned) / sizeof(*owned));
		owned[tokens++] = *token;
		live_tokens++;
	}
	return rest;
}

void __real_free(void *);
void __wrap_free(void *pointer)
{
	if (pointer)
		for (unsigned i = 0; i < tokens; i++)
			if (owned[i] == pointer)
			{
				owned[i] = NULL;
				assert(live_tokens > 0);
				live_tokens--;
				break;
			}
	__real_free(pointer);
}

void __real_MyXGrabServer(Display *);
void __wrap_MyXGrabServer(Display *display)
{
	grabs += measuring;
	__real_MyXGrabServer(display);
}

static void command(const exec_context_t *exc, const char *text)
{
	execute_function_override_window(NULL, exc, (char *)text, NULL,
		FUNC_DONT_DEFER, NULL);
}

static void observe(const exec_context_t *exc, const char *text,
	unsigned count, Bool baseline)
{
	memset(owned, 0, sizeof(owned));
	tokens = live_tokens = 0;
	tracking = True;
	command(exc, text);
	tracking = False;
	printf("STYLE_COMMAND_OWNER tokens=%u retained=%u expected=%u\n",
		tokens, live_tokens, count);
	fflush(stdout);
	assert(tokens == count);
	assert(live_tokens == (baseline ? count : 0));
	/* Baseline observations deliberately retain the original leaked tokens
	 * until this isolated process exits; no production pointer is freed. */
	memset(owned, 0, sizeof(owned));
	tokens = live_tokens = 0;
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
	Bool baseline = getenv("FVWM_STYLE_COMMAND_BASELINE") != NULL;
	FvwmWindow *window;
	window_style style;
	exec_context_changes_t changes = { 0 };
	const exec_context_t *context;

	alarm(45);
	fFvwmInStartup = False;
	command(exc, "Style * NoTitle, BorderWidth 3, HandleWidth 3");
	command(exc, "UpdateStyles");
	window = create_window(exc);
	command(exc, "Style owned-template-compat BorderWidth 7, HandleWidth 7");
	observe(exc, "Style (Name \"update barrier control\") "
		"UseStyle owned-template-compat", 1, baseline);
	command(exc, "DestroyStyle owned-template-compat");
	command(exc, "UpdateStyles");
	check_border(window, 7);
	puts("use-style-copy-survives-source-removal: passed");

	command(exc, "Style (Name owned-template-explicit) "
		"BorderWidth 9, HandleWidth 9");
	observe(exc, "Style (Name \"update barrier control\") "
		"UseStyle (Name owned-template-explicit)", 1, baseline);
	command(exc, "UpdateStyles");
	check_border(window, 9);
	puts("use-style-explicit-selector-release: passed");

	observe(exc, "Style (Name \"update barrier control\") "
		"UseStyle owned-template-missing, BorderWidth 5, HandleWidth 5",
		1, baseline);
	command(exc, "UpdateStyles");
	check_border(window, 5);
	puts("use-style-miss-preserves-following-options: passed");

	command(exc, "Style owned-template-destroy BorderWidth 11");
	command(exc, "Style (Resource owned-template-r, Class owned-template-c, "
		"Name owned-template-n, Icon owned-template-i) BorderWidth 10");
	observe(exc, "DestroyStyle owned-template-destroy", 1, baseline);
	observe(exc, "DestroyStyle (Resource owned-template-r, "
		"Class owned-template-c, Name owned-template-n, "
		"Icon owned-template-i)", 4, baseline);
	command(exc, "UpdateStyles");
	assert(!Scr.flags.do_need_window_update);
	observe(exc, "DestroyStyle owned-template-missing", 1, baseline);
	assert(!Scr.flags.do_need_window_update);
	puts("destroy-style-found-and-missing-selector-release: passed");

	observe(exc, "DestroyStyle (Name \"update barrier control\")", 0,
		baseline);
	assert(IS_STYLE_DELETED(window));
	assert(Scr.flags.do_need_window_update);
	command(exc, "UpdateStyles");
	check_border(window, 3);
	lookup_style(window, &style);
	assert(!style.has_style_changed);
	puts("destroy-style-invalidates-and-applies-fallback: passed");

	for (unsigned mode = 0; mode < 3; mode++)
	{
		assert(!Scr.flags.do_need_window_update);
		if (mode == 0)
			command(exc, "Style (Name missing-close");
		else if (mode == 1)
			command(exc, "FocusStyle (Name missing-close");
		else
		{
			changes.w.fw = window;
			context = exc_clone_context(exc, &changes, ECC_FW);
			CMD_WindowStyle(NULL, context, NULL, NULL);
			exc_destroy_context(context);
		}
		assert(Scr.flags.do_need_window_update == baseline);
		grabs = 0;
		measuring = True;
		command(exc, "UpdateStyles");
		measuring = False;
		assert(grabs == (baseline ? 1U : 0U));
		check_border(window, 3);
		printf("STYLE_COMMAND_OWNER refused=%u grabs=%u\n", mode, grabs);
	}
	puts("refused-style-commands-do-not-schedule-pass: passed");
	if (!baseline)
	{
		command(exc, "Style");
		command(exc, "FocusStyle");
		CMD_Style(NULL, exc, NULL, NULL);
		CMD_FocusStyle(NULL, exc, NULL, NULL);
		assert(!Scr.flags.do_need_window_update);
		command(exc, "Style (Name \"update barrier control\") "
			"BorderWidth 4, HandleWidth 4");
		command(exc, "UpdateStyles");
		check_border(window, 4);
		command(exc, "DestroyStyle (Name \"update barrier control\")");
		command(exc, "UpdateStyles");
		check_border(window, 3);
		puts("missing-selector-command-engine-refusal: passed");
	}
	command(exc, "Style * BorderWidth 6, HandleWidth 6");
	command(exc, "Style (Name missing-close");
	assert(Scr.flags.do_need_window_update);
	command(exc, "UpdateStyles");
	check_border(window, 6);
	puts("refused-command-preserves-pending-style-update: passed");
	puts("style command ownership: passed");
	fflush(stdout);
	_exit(0);
}
