/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/module_records.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "libs/wcontext.h"
#include "fvwm/fvwm.h"
#include "fvwm/add_window.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "fvwm/stack.h"
#include "fvwm/module_list.h"
#include "fvwm/initial_window_options.h"
static FvwmWindow *victim;
static unsigned mode, removed, raises;
static Bool inside, ready;
void __wrap_focus_grab_buttons_on_layer(int layer)
{
}

/* PositiveWrite can execute a module command during its unlock wait. Use the
 * actual window lifecycle here; transport/deadline tests cover that wait. */
void __wrap_PositiveWrite(fmodule *module, unsigned long *packet, int size)
{
	unsigned message = mode == 1 ? M_LOWER_WINDOW :
		mode == 2 || mode == 3 ? M_RESTACK : M_RAISE_WINDOW;

	if (ready && !inside && packet[1] == M_RAISE_WINDOW)
	{
		raises++;
	}
	if (ready && !inside && packet[1] == message &&
	    ((mode != 4 && mode != 5) || raises == 2))
	{
		inside = True;
		destroy_window(victim);
		removed++;
	}
}

static FvwmWindow *create_managed_window(
	const exec_context_t *exc, Window *client)
{
	XSizeHints hints = { 0 };
	exec_context_changes_t changes = { 0 };
	initial_window_options_t options = { 0 };
	const char *map_command = NULL;
	const exec_context_t *context;
	FvwmWindow *window;

	/* Startup may already own a server grab: reuse its connection. */
	*client = XCreateSimpleWindow(dpy, Scr.Root,
		10, 10, 100, 80, 0, 0, 0);
	XStoreName(dpy, *client, "stack retirement control");
	hints.flags = USPosition | USSize;
	hints.x = hints.y = 10;
	hints.width = 100;
	hints.height = 80;
	XSetWMNormalHints(dpy, *client, &hints);
	XSync(dpy, False);
	changes.w.w = *client;
	changes.w.wcontext = C_ROOT;
	context = exc_clone_context(exc, &changes,
		ECC_W | ECC_FW | ECC_WCONTEXT);
	options.initial_state = NormalState;
	window = AddWindow(&map_command, context, NULL, &options);
	assert(window != NULL && FW_W(window) == *client);
	exc_destroy_context(context);
	return window;
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	FvwmWindow *windows[70];
	Window clients[70];
	unsigned count, victim_index, survivors = 0;
	FvwmWindow *target;

	mode = (unsigned)strtoul(getenv("FVWM_RETIREMENT_MODE"), NULL, 10);
	assert(mode <= 6);
	count = mode == 5 ? 70 : mode >= 4 ? 4 : 3;
	for (unsigned i = 0; i < count; i++)
	{
		windows[i] = create_managed_window(exc, clients + i);
	}
	if (mode >= 4)
	{
		for (unsigned i = 0; i + 1 < count; i++)
		{
			SET_TRANSIENT(windows[i], 1);
			windows[i]->flags.common.s.do_stack_transient_parent = 1;
			windows[i]->flags.common.s.do_raise_transient = 0;
			windows[i]->flags.common.s.do_lower_transient = 0;
			FW_W_TRANSIENTFOR(windows[i]) = clients[i + 1];
		}
	}
	if (mode == 3)
	{
		for (unsigned i = 0; i + 1 < count; i++)
		{
			SET_TRANSIENT(windows[i], 1);
			FW_W_TRANSIENTFOR(windows[i]) = clients[count - 1];
		}
		windows[count - 1]->flags.common.s.do_raise_transient = 1;
	}
	target = windows[mode == 3 ? count - 1 : 0];
	victim_index = mode == 5 ? 33 : mode == 4 ? 2 :
		mode == 6 || mode == 3 ? count - 1 : 0;
	victim = windows[victim_index];
	assert(!Scr.flags.is_executing_complex_function);
	assert(!Scr.flags.is_executing_menu_function);
	module_register(module_create());
	ready = True;
	if (mode == 1)
	{
		LowerWindow(target, False);
	}
	else if (mode == 2)
	{
		RestackWindow(target, False);
	}
	else
	{
		RaiseWindow(target, False);
	}
	assert(removed == 1);
	assert(find_managed_client(clients[victim_index]) == NULL);
	for (FvwmWindow *window = Scr.FvwmRoot.next;
	     window != NULL; window = window->next)
	{
		survivors++;
	}
	assert(survivors == count - 1);
	puts("stack-retirement-survivor-continuation: passed");
	fflush(stdout);
	_exit(0);
}
