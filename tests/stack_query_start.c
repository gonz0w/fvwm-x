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
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "fvwm/stack.h"
#include "libs/FScreen.h"

#define CHILD_COUNT 108
static FvwmWindow target;
static Window children[CHILD_COUNT];
static Window highest, lower, hidden, failed, input_only, selected;
static unsigned queries, attributes;
static Bool active, fail_tree, absent_target;
extern Status __real_XQueryTree(Display *, Window, Window *, Window *,
	Window **, unsigned *);
extern Status __real_XGetWindowAttributes(Display *, Window,
	XWindowAttributes *);
extern int __real_XConfigureWindow(Display *, Window, unsigned,
	XWindowChanges *);
extern Bool __real_is_frame_hide_window(Window);

Status __wrap_XQueryTree(Display *display, Window window, Window *root,
	Window *parent, Window **result, unsigned *count)
{
	if (!active || window != Scr.Root)
	{
		return __real_XQueryTree(display, window, root, parent, result, count);
	}
	queries++;
	*result = NULL;
	*count = 0;
	if (fail_tree)
	{
		return 0;
	}
	*result = malloc(sizeof(children));
	assert(*result != NULL);
	memcpy(*result, children, sizeof(children));
	if (absent_target)
	{
		(*result)[0] = 0x50000000;
	}
	*root = Scr.Root;
	*parent = None;
	*count = CHILD_COUNT;
	return 1;
}

Status __wrap_XGetWindowAttributes(Display *display, Window window,
	XWindowAttributes *value)
{
	if (!active)
	{
		return __real_XGetWindowAttributes(display, window, value);
	}
	attributes++;
	if (window == failed)
	{
		return 0;
	}
	memset(value, 0, sizeof(*value));
	value->class = window == input_only ? InputOnly : InputOutput;
	value->override_redirect = window == highest || window == lower ||
		window == hidden || window == Scr.NoFocusWin || window == input_only;
	return 1;
}

Bool __wrap_is_frame_hide_window(Window window)
{
	return active ? window == hidden : __real_is_frame_hide_window(window);
}

int __wrap_XConfigureWindow(Display *display, Window window, unsigned mask,
	XWindowChanges *changes)
{
	if (!active)
	{
		return __real_XConfigureWindow(display, window, mask, changes);
	}
	assert(window == FW_W_FRAME(&target));
	assert(mask == (CWSibling | CWStackMode));
	assert(changes->stack_mode == Above);
	selected = changes->sibling;
	return 1;
}

void __wrap_focus_grab_buttons_on_layer(int layer)
{
	/* Query selection controls do not inspect focus button policy. */
}

static void check_raise(Window expected, unsigned expected_attributes)
{
	queries = attributes = 0;
	selected = None;
	RaiseWindow(&target, False);
	assert(selected == expected);
	assert(queries == (target.Desk == target.m->virtual_scr.CurrentDesk ? 1 : 0));
#ifdef STACK_QUERY_BASELINE
	printf("baseline selected=%lu attributes=%u\n", selected, attributes);
#else
	assert(attributes == expected_attributes);
#endif
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	assert(Scr.FvwmRoot.next == NULL);
	target.m = monitor_get_current();
	assert(target.m != NULL);
	target.layer = 4;
	target.Desk = target.m->virtual_scr.CurrentDesk;
	FW_W(&target) = XCreateSimpleWindow(dpy, Scr.Root, 0, 0, 10, 10, 0, 0, 0);
	FW_W_FRAME(&target) = FW_W(&target);
	target.prev = &Scr.FvwmRoot;
	target.stack_next = target.stack_prev = &Scr.FvwmRoot;
	Scr.FvwmRoot.next = &target;
	Scr.FvwmRoot.stack_next = Scr.FvwmRoot.stack_prev = &target;
	Scr.bo.do_raise_over_unmanaged = 1;
	for (unsigned index = 0; index < CHILD_COUNT; index++)
	{
		children[index] = 0x60000000UL + index;
	}
	children[0] = FW_W_FRAME(&target);
	lower = children[1];
	highest = children[103];
	input_only = children[104];
	failed = children[105];
	children[106] = Scr.NoFocusWin;
	hidden = children[107];
	active = True;
	check_raise(highest, 3);
	puts("highest-override-input-only-failure-and-internal-exclusions: passed");
	highest = None;
	check_raise(lower, 105);
	lower = None;
	check_raise(None, 106);
	puts("no-override-complete-scan: passed");
	absent_target = True;
	check_raise(None, 0);
	absent_target = False;
	fail_tree = True;
	check_raise(None, 0);
	fail_tree = False;
	target.Desk++;
	check_raise(None, 0);
	puts("absent-target-tree-failure-and-off-desk: passed");
	fflush(stdout);
	_exit(0);
}
