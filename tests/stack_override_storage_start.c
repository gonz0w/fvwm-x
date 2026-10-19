/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "fvwm/stack.h"
#include "libs/monitor_types.h"

static FvwmWindow target, higher;
static Window popup;
static Bool refuse, refusal_seen;
void *__real_malloc(size_t size);

void *__wrap_malloc(size_t size)
{
	if (refuse && size == sizeof(Window))
	{
		refuse = False;
		refusal_seen = True;
		errno = ENOMEM;
		return NULL;
	}
	return __real_malloc(size);
}

void __wrap_focus_grab_buttons_on_layer(int layer)
{
}

static Bool target_is_above(void)
{
	Window root, parent, *children;
	unsigned count, frame_index = 0, popup_index = 0, found = 0;
	unsigned higher_index = 0;

	XSync(dpy, False);
	assert(XQueryTree(dpy, Scr.Root, &root, &parent, &children, &count));
	for (unsigned i = 0; i < count; i++)
	{
		if (children[i] == FW_W_FRAME(&target))
		{
			frame_index = i;
			found++;
		}
		if (FW_W_FRAME(&higher) != None &&
		    children[i] == FW_W_FRAME(&higher))
		{
			higher_index = i;
			found++;
		}
		if (children[i] == popup)
		{
			popup_index = i;
			found++;
		}
	}
	XFree(children);
	assert(found == (FW_W_FRAME(&higher) == None ? 2 : 3));
	if (FW_W_FRAME(&higher) != None &&
	    target.stack_prev == &Scr.FvwmRoot)
	{
		assert(frame_index > higher_index);
	}
	return frame_index > popup_index;
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	FvwmWindow original, higher_original;
	Bool two = getenv("FVWM_OVERRIDE_TWO") != NULL;
	XSetWindowAttributes attributes = { .override_redirect = True };

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
	if (two)
	{
		higher.m = target.m;
		higher.layer = target.layer;
		higher.Desk = target.Desk;
		FW_W(&higher) = XCreateSimpleWindow(
			dpy, Scr.Root, 10, 10, 10, 10, 0, 0, 0);
		FW_W_FRAME(&higher) = FW_W(&higher);
		higher.prev = &Scr.FvwmRoot;
		higher.next = &target;
		target.prev = &higher;
		higher.stack_prev = &Scr.FvwmRoot;
		higher.stack_next = &target;
		target.stack_prev = &higher;
		Scr.FvwmRoot.next = &higher;
		Scr.FvwmRoot.stack_next = &higher;
	}
	Scr.bo.do_raise_over_unmanaged = 1;
	popup = XCreateSimpleWindow(dpy, Scr.Root, 20, 20, 10, 10, 0, 0, 0);
	XChangeWindowAttributes(dpy, popup, CWOverrideRedirect, &attributes);
	XMapWindow(dpy, FW_W_FRAME(&target));
	if (two)
	{
		XMapWindow(dpy, FW_W_FRAME(&higher));
	}
	XMapWindow(dpy, popup);
	XRaiseWindow(dpy, popup);
	assert(!target_is_above());
	original = target;
	higher_original = higher;
	if (two)
	{
		original.stack_prev = &Scr.FvwmRoot;
		original.stack_next = &higher;
		higher_original.stack_prev = &target;
		higher_original.stack_next = &Scr.FvwmRoot;
	}
	refuse = getenv("FVWM_OVERRIDE_REFUSE") != NULL;
	RaiseWindow(&target, False);
	assert(memcmp(&target, &original, sizeof(target)) == 0);
	assert(refusal_seen == (getenv("FVWM_OVERRIDE_REFUSE") != NULL));
	assert(target_is_above() == !refusal_seen);
	assert(Scr.FvwmRoot.stack_next == &target);
	assert(Scr.FvwmRoot.stack_prev == (two ? &higher : &target));
	if (two)
	{
		assert(memcmp(&higher, &higher_original, sizeof(higher)) == 0);
		puts("completed-managed-raise-retained: passed");
	}
	if (refusal_seen)
	{
		puts("override-storage-refusal-"
			"preserves-managed-and-server-order: passed");
		RaiseWindow(&target, False);
		assert(target_is_above());
		assert(memcmp(&target, &original, sizeof(target)) == 0);
		puts("override-storage-refusal-retry: passed");
	}
	else
	{
		puts("ordinary-override-raise: passed");
	}
	fflush(stdout);
	_exit(0);
}
