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
#include <unistd.h>
#include <X11/Xatom.h>
#include "libs/wcontext.h"
#include "fvwm/fvwm.h"
#include "fvwm/add_window.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/functions.h"
#include "fvwm/screen.h"
#include "fvwm/style.h"

static Bool measuring;
static unsigned long comparisons, matches, grabbed_matches, grabs;
static unsigned grab_depth;
static uint64_t grabbed_ns, grab_started;

static uint64_t now_ns(void)
{
	struct timespec value;

	assert(clock_gettime(CLOCK_MONOTONIC, &value) == 0);
	return (uint64_t)value.tv_sec * 1000000000 + value.tv_nsec;
}

int __real_strcmp(const char *, const char *);
int __wrap_strcmp(const char *a, const char *b)
{
	/* Only count comparisons between the uniquely named workload rules.
	 * Command parsing/setup and unrelated core strings stay out of scope. */
	if (measuring && strncmp(a, "probe-", 6) == 0 &&
	    strncmp(b, "probe-", 6) == 0)
	{
		comparisons++;
	}
	return __real_strcmp(a, b);
}

int __real_matchWildcards(const char *, const char *);
int __wrap_matchWildcards(const char *pattern, const char *value)
{
	if (measuring && pattern && strncmp(pattern, "probe-", 6) == 0)
	{
		matches++;
		if (grab_depth != 0)
		{
			grabbed_matches++;
		}
	}
	return __real_matchWildcards(pattern, value);
}

void __real_MyXGrabServer(Display *);
void __wrap_MyXGrabServer(Display *display)
{
	__real_MyXGrabServer(display);
	if (measuring && grab_depth++ == 0)
	{
		grabs++;
		grab_started = now_ns();
	}
}

void __real_MyXUngrabServer(Display *);
void __wrap_MyXUngrabServer(Display *display)
{
	if (measuring)
	{
		assert(grab_depth != 0);
		if (--grab_depth == 0)
		{
			grabbed_ns += now_ns() - grab_started;
		}
	}
	__real_MyXUngrabServer(display);
}

static void command(const exec_context_t *exc, const char *text)
{
	execute_function_override_window(NULL, exc, (char *)text, NULL,
		FUNC_DONT_DEFER, NULL);
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

	XStoreName(dpy, client, "style profile control");
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

static void lookup_control(FvwmWindow *window, unsigned count)
{
	FvwmWindow sample = *window;
	window_style style;
	char name[64];

	snprintf(name, sizeof(name), "probe-%04u", count - 1);
	/* Borrow the live record's initialized EWMH/monitor state only for this
	 * synchronous lookup. Never publish this temporary identity. */
	sample.class.res_name = name;
	sample.class.res_class = "unmatched class";
	sample.name.name = "unmatched name";
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 7);
}

static void check_precedence(const exec_context_t *exc, FvwmWindow *window)
{
	FvwmWindow sample = *window;
	window_style style;

	sample.class.res_name = "precedence-window";
	sample.class.res_class = "unmatched class";
	sample.name.name = "unmatched name";
	command(exc, "Style precedence-* BorderWidth 4, HandleWidth 4");
	command(exc, "Style precedence-window BorderWidth 7");
	command(exc, "Style precedence-* HandleWidth 9");
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 7 && SGET_HANDLE_WIDTH(style) == 9);
	simplify_style_list();
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 7 && SGET_HANDLE_WIDTH(style) == 9);
	command(exc, "DestroyStyle precedence-window");
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 4 && SGET_HANDLE_WIDTH(style) == 9);
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	const char *mode = getenv("FVWM_STYLE_PROFILE_MODE");
	const char *size = getenv("FVWM_STYLE_PROFILE_SIZE");
	const char *cohort = getenv("FVWM_STYLE_PROFILE_WINDOWS");
	unsigned count, windows;
	FvwmWindow *clients[32];
	char text[128];
	uint64_t started, elapsed;

	alarm(30);
	assert(mode && size && cohort);
	count = (unsigned)strtoul(size, NULL, 10);
	assert(count == 64 || count == 128 || count == 256);
	fFvwmInStartup = False;
	command(exc, "Style * NoTitle, BorderWidth 3, HandleWidth 3");
	command(exc, "UpdateStyles");
	windows = (unsigned)strtoul(cohort, NULL, 10);
	assert(windows == 1 || windows == 8 || windows == 16 || windows == 32);
	for (unsigned i = 0; i < windows; i++)
	{
		clients[i] = create_window(exc);
		check_border(clients[i], 3);
	}
	for (unsigned i = 0; i < count; i++)
	{
		snprintf(text, sizeof(text), "Style probe-%04u BorderWidth %u",
			i, i == count - 1 ? 7 : 5);
		command(exc, text);
	}
	if (strcmp(mode, "update") == 0)
	{
		command(exc, "Style * BorderWidth 7, HandleWidth 7");
		/* Pending rules must not alter existing clients before the barrier. */
		for (unsigned i = 0; i < windows; i++)
		{
			check_border(clients[i], 3);
		}
	}
	measuring = True;
	started = now_ns();
	if (strcmp(mode, "simplify") == 0)
	{
		simplify_style_list();
	}
	else if (strcmp(mode, "lookup") == 0)
	{
		for (unsigned i = 0; i < 32; i++)
		{
			lookup_control(clients[0], count);
		}
	}
	else
	{
		assert(strcmp(mode, "update") == 0);
		command(exc, "UpdateStyles");
	}
	elapsed = now_ns() - started;
	measuring = False;
	assert(grab_depth == 0);
	if (strcmp(mode, "simplify") == 0)
	{
		assert(comparisons == (unsigned long)count * (count - 1) / 2);
		assert(!Scr.flags.do_need_style_list_update);
		lookup_control(clients[0], count);
		check_precedence(exc, clients[0]);
	}
	else if (strcmp(mode, "lookup") == 0)
	{
		assert(matches == 32 * (3 * count - 2));
	}
	else
	{
		assert(!Scr.flags.do_need_window_update);
		assert(grabs == 1 && grabbed_matches == 3 * count * windows);
		assert(matches == grabbed_matches);
		for (unsigned i = 0; i < windows; i++)
		{
			check_border(clients[i], 7);
		}
		measuring = True;
		command(exc, "UpdateStyles");
		measuring = False;
		assert(grabs == 1 && matches == grabbed_matches);
	}
	printf("STYLE_PROFILE {\"mode\":\"%s\",\"rules\":%u,"
		"\"windows\":%u,\"comparisons\":%lu,\"matches\":%lu,"
		"\"grabbed_matches\":%lu,\"grabs\":%lu,"
		"\"elapsed_ns\":%llu,\"grabbed_ns\":%llu}\n",
		mode, count, windows, comparisons, matches, grabbed_matches,
		grabs, (unsigned long long)elapsed, (unsigned long long)grabbed_ns);
	fflush(stdout);
	_exit(0);
}
