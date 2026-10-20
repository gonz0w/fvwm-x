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

static FvwmWindow *survivor;
static Window retired_client;
static Bool checking_boundary;
static unsigned completed;

static void command(const exec_context_t *exc, const char *text,
	FvwmWindow *window)
{
	execute_function_override_window(NULL, exc, (char *)text, NULL,
		FUNC_DONT_DEFER, window);
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

static void passed(const char *name)
{
	printf("%s: passed\n", name);
	completed++;
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

void __real_module_config_dispatch_ready(void);

/* Observe the real outer loop immediately after its update barrier, before
 * config continuations/readiness/event dispatch can observe managed state.
 * No production update, destruction or X property operation is mocked. */
void __wrap_module_config_dispatch_ready(void)
{
	XEvent event;

	if (!checking_boundary)
	{
		__real_module_config_dispatch_ready();
		return;
	}
	assert(!Scr.flags.is_window_scheduled_for_destroy);
	assert(Scr.FWScheduledForDestroy == NULL);
	assert(find_managed_client(retired_client) == NULL);
	assert(Scr.FvwmRoot.next == survivor && survivor->next == NULL);
	assert(!Scr.flags.do_need_window_update);
	check_border(survivor, 9);
	passed("outer-boundary-retirement-before-style-publication");
	assert(XCheckTypedWindowEvent(dpy, Scr.NoFocusWin, ClientMessage,
		&event));
	assert(event.xclient.data.l[0] == 12345);
	passed("outer-update-with-input-still-pending");
	assert(completed == 8);
	puts("window update barriers: passed");
	fflush(stdout);
	_exit(0);
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	FvwmWindow *other;
	XEvent pending = { .xclient = { .type = ClientMessage } };

	alarm(20);
	fFvwmInStartup = False;
	command(exc, "Style * NoTitle, BorderWidth 3, HandleWidth 3", NULL);
	command(exc, "UpdateStyles", NULL);
	survivor = create_window(exc);
	other = create_window(exc);
	check_border(survivor, 3);
	check_border(other, 3);
	passed("initial-managed-state");

	command(exc, "Style * BorderWidth 5, HandleWidth 5", NULL);
	assert(Scr.flags.do_need_window_update);
	check_border(survivor, 3);
	passed("style-command-defers-existing-window-update");
	command(exc, "UpdateStyles", NULL);
	assert(!Scr.flags.do_need_window_update);
	check_border(survivor, 5);
	check_border(other, 5);
	command(exc, "UpdateStyles", NULL);
	check_border(survivor, 5);
	passed("explicit-update-and-clean-repeat");

	command(exc, "Style * BorderWidth 6, HandleWidth 6", NULL);
	command(exc, "Refresh", NULL);
	assert(!Scr.flags.do_need_window_update);
	check_border(survivor, 6);
	check_border(other, 6);
	passed("refresh-commits-styles-before-return");

	command(exc, "Style * BorderWidth 7, HandleWidth 7", NULL);
	command(exc, "RefreshWindow", survivor);
	assert(!Scr.flags.do_need_window_update);
	check_border(survivor, 7);
	check_border(other, 7);
	passed("refresh-window-commits-global-pending-styles");

	/* Retirement removes identity/list membership immediately but keeps the
	 * record alive while a complex command still borrows it. */
	retired_client = FW_W(other);
	Scr.flags.is_executing_complex_function = 1;
	destroy_window(other);
	assert(IS_SCHEDULED_FOR_DESTROY(other));
	assert(find_managed_client(retired_client) == NULL);
	assert(Scr.flags.is_window_scheduled_for_destroy);
	destroy_scheduled_windows();
	assert(Scr.flags.is_window_scheduled_for_destroy);
	Scr.flags.is_executing_complex_function = 0;
	passed("retired-window-record-survives-borrowed-command");

	command(exc, "Style * BorderWidth 8, HandleWidth 8", NULL);
	command(exc, "Style * BorderWidth 9, HandleWidth 9", NULL);
	check_border(survivor, 7);
	pending.xclient.display = dpy;
	pending.xclient.window = Scr.NoFocusWin;
	pending.xclient.format = 32;
	pending.xclient.data.l[0] = 12345;
	XPutBackEvent(dpy, &pending);
	checking_boundary = True;
	HandleEvents();
	assert(0);
}
