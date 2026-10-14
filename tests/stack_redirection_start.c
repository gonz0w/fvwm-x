/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "fvwm/stack.h"
#include "libs/FScreen.h"

#define WINDOW_COUNT 4
static FvwmWindow windows[WINDOW_COUNT];

void __wrap_focus_grab_buttons_on_layer(int layer)
{
	/* Selection/ring controls leave focus button policy to real-window tests. */
}

static void reset_windows(unsigned prefix, unsigned length, Bool groups)
{
	Window frames[WINDOW_COUNT];

	Scr.FvwmRoot.scratch.i = 0;
	Scr.FvwmRoot.next = windows;
	Scr.FvwmRoot.stack_next = windows + WINDOW_COUNT - 1;
	Scr.FvwmRoot.stack_prev = windows;
	for (unsigned i = 0; i < WINDOW_COUNT; i++)
	{
		windows[i].prev = i == 0 ? &Scr.FvwmRoot : windows + i - 1;
		windows[i].next = i + 1 == WINDOW_COUNT ? NULL : windows + i + 1;
		windows[i].stack_prev = i + 1 == WINDOW_COUNT ? &Scr.FvwmRoot :
			windows + i + 1;
		windows[i].stack_next = i == 0 ? &Scr.FvwmRoot : windows + i - 1;
		SET_IN_TRANSIENT_SUBTREE(windows + i, 0);
		windows[i].scratch.p = NULL;
		SET_TRANSIENT(windows + i, i < prefix + length);
		windows[i].flags.common.s.do_stack_transient_parent = 1;
		windows[i].flags.common.s.do_raise_transient = groups;
		windows[i].flags.common.s.do_lower_transient = groups;
		FW_W_TRANSIENTFOR(windows + i) = None;
		frames[WINDOW_COUNT - 1 - i] = FW_W_FRAME(windows + i);
	}
	for (unsigned i = 0; i < prefix + length; i++)
	{
		FW_W_TRANSIENTFOR(windows + i) = FW_W(windows +
			(i + 1 == prefix + length ? prefix : i + 1));
	}
	XRestackWindows(dpy, frames, WINDOW_COUNT);
}

static void observe_ring(Window order[WINDOW_COUNT])
{
	FvwmWindow *current = Scr.FvwmRoot.stack_next;

	for (unsigned i = 0; i < WINDOW_COUNT; i++)
	{
		assert(current != &Scr.FvwmRoot);
		assert(current->stack_next->stack_prev == current);
		order[i] = FW_W(current);
		current = current->stack_next;
	}
	assert(current == &Scr.FvwmRoot);
	/* Check the real server's frame order against the authoritative ring. */
	Window root, parent, *children;
	unsigned count, position = 0;
	assert(XQueryTree(dpy, Scr.Root, &root, &parent, &children, &count));
	for (unsigned i = count; i-- > 0;)
	{
		for (unsigned j = 0; j < WINDOW_COUNT; j++)
		{
			if (children[i] == FW_W_FRAME(windows + j))
			{
				assert(position < WINDOW_COUNT &&
					order[position++] == FW_W(windows + j));
			}
		}
	}
	XFree(children);
	assert(position == WINDOW_COUNT);
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	void (*operations[])(FvwmWindow *, Bool) = {
		RaiseWindow, LowerWindow, RestackWindow};
	for (unsigned i = 0; i < WINDOW_COUNT; i++)
	{
		FW_W(windows + i) = XCreateSimpleWindow(dpy, Scr.Root,
			0, 0, 10, 10, 0, 0, 0);
		FW_W_FRAME(windows + i) = FW_W(windows + i);
		windows[i].m = monitor_get_current();
		assert(windows[i].m != NULL);
		windows[i].Desk = windows[i].m->virtual_scr.CurrentDesk;
		windows[i].layer = 4;
	}
	for (unsigned prefix = 0; prefix < 2; prefix++)
	{
		for (unsigned length = 2; length < 4; length++)
		{
			for (unsigned groups = 0; groups < 2; groups++)
			{
				for (unsigned op = 0; op < 3; op++)
				{
					Window actual[WINDOW_COUNT], direct[WINDOW_COUNT];
					reset_windows(prefix, length, groups);
					operations[op](windows, False);
					observe_ring(actual);
					for (unsigned i = 0; i < prefix + length; i++)
					{
						assert(IS_TRANSIENT(windows + i));
						assert(FW_W_TRANSIENTFOR(windows + i) ==
							FW_W(windows + (i + 1 == prefix + length ?
							prefix : i + 1)));
					}
					reset_windows(prefix, length, groups);
					windows[0].flags.common.s.do_stack_transient_parent = 0;
					operations[op](windows, False);
					observe_ring(direct);
					for (unsigned i = 0; i < WINDOW_COUNT; i++)
					{
						assert(actual[i] == direct[i]);
					}
				}
			}
		}
	}
	puts("cyclic-redirection-matches-direct-target-and-transient-rules: passed");
	reset_windows(0, 2, False);
	SET_TRANSIENT(windows + 1, 0);
	RaiseWindow(windows, False);
	assert(Scr.FvwmRoot.stack_next == windows);
	puts("acyclic-redirection-and-following-operation: passed");
	fflush(stdout);
	_exit(0);
}
