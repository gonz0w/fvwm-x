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
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/screen.h"
#include "fvwm/stack.h"
#include "fvwm/externs.h"

static Bool fail_worklist;
void *__real_calloc(size_t count, size_t size);

void *__wrap_calloc(size_t count, size_t size)
{
	if (fail_worklist)
	{
		fail_worklist = False;
		return NULL;
	}
	return __real_calloc(count, size);
}


static void setup(FvwmWindow *windows, unsigned count, unsigned shape)
{
	Scr.FvwmRoot.next = windows;
	Scr.FvwmRoot.stack_next = windows;
	Scr.FvwmRoot.stack_prev = windows + count - 1;
	Scr.FvwmRoot.scratch.i = 0;
	for (unsigned i = 0; i < count; i++)
	{
		FW_W(windows + i) = 0x70000000UL + i;
	}
	for (unsigned i = 0; i < count; i++)
	{
		FvwmWindow *w = windows + i;

		FW_W(w) = 0x70000000UL + i;
		w->prev = i == 0 ? &Scr.FvwmRoot : w - 1;
		w->next = i + 1 == count ? NULL : w + 1;
		w->stack_prev = i == 0 ? &Scr.FvwmRoot : w - 1;
		w->stack_next = i + 1 == count ? &Scr.FvwmRoot : w + 1;
		if (shape == 0 && i > 0)
		{
			SET_TRANSIENT(w, 1);
			FW_W_TRANSIENTFOR(w) = FW_W(w - 1);
		}
		else if (shape != 0 && i + 1 < count)
		{
			SET_TRANSIENT(w, 1);
			FW_W_TRANSIENTFOR(w) = shape == 1 ?
				FW_W(windows + count - 1) : FW_W(w + 1);
		}
		assert(XSaveContext(dpy, FW_W(w), FvwmContext, (char *)w) == 0);
	}
}

static void test_closure(
	unsigned count, unsigned shape, Bool fail, Bool use_groups)
{
	FvwmWindow *windows = calloc(count, sizeof(*windows));

	assert(windows);
	setup(windows, count, shape);
	for (unsigned i = 0; i < count; i++)
	{
		FvwmWindow *expected = NULL;
		if (shape == 0 && i > 0)
		{
			expected = windows + i - 1;
		}
		else if (shape != 0 && i + 1 < count)
		{
			expected = shape == 1 ? windows + count - 1 : windows + i + 1;
		}
		assert(get_transientfor_fvwmwindow(windows + i) == expected);
	}
	fail_worklist = fail;
	mark_transient_subtree(shape == 0 ? windows : windows + count - 1,
		MARK_ALL_LAYERS, MARK_ALL, False, use_groups);
	assert(!fail_worklist);
	for (unsigned i = 0; i < count; i++)
	{
		assert(IS_IN_TRANSIENT_SUBTREE(windows + i));
		assert(windows[i].scratch.i == (shape == 0 ? i :
			shape == 1 ? (i + 1 == count ? 0 : 1) :
			count - i - 1));
	}
	for (unsigned i = 0; i < count; i++)
	{
		XDeleteContext(dpy, FW_W(windows + i), FvwmContext);
	}
	free(windows);
	Scr.FvwmRoot.next = NULL;
	Scr.FvwmRoot.stack_next = Scr.FvwmRoot.stack_prev = &Scr.FvwmRoot;
}

static void test_cached_layer_marks(void)
{
	FvwmWindow windows[24] = { 0 };

	setup(windows, 24, 2);
	for (unsigned i = 0; i < 24; i++)
	{
		windows[i].layer = 6;
		windows[i].scratch.p = get_transientfor_fvwmwindow(windows + i);
	}
	SET_IN_TRANSIENT_SUBTREE(windows + 12, 1);
	windows[12].scratch.i = 200000;
	Scr.FvwmRoot.scratch.i = 1;
	mark_transient_subtree(windows + 23, 6, MARK_ALL, False, False);
	assert(Scr.FvwmRoot.scratch.i == 0);
	for (unsigned i = 0; i < 24; i++)
	{
		assert(IS_IN_TRANSIENT_SUBTREE(windows + i));
		assert(windows[i].scratch.i == (i <= 12 ? 200000 + 12 - i :
			23 - i));
		XDeleteContext(dpy, FW_W(windows + i), FvwmContext);
	}
	Scr.FvwmRoot.next = NULL;
	Scr.FvwmRoot.stack_next = Scr.FvwmRoot.stack_prev = &Scr.FvwmRoot;
	puts("cached-layer-marks-retain-ranks: passed");
}

static void test_window_groups(void)
{
	FvwmWindow windows[8] = { 0 };
	XWMHints hints[8] = { 0 };

	setup(windows, 8, 0);
	for (unsigned i = 0; i < 8; i++)
	{
		SET_TRANSIENT(windows + i, 0);
		windows[i].wmhints = hints + i;
		hints[i].flags = WindowGroupHint;
		hints[i].window_group = FW_W(windows + 7);
		FW_COMMON_STATIC_FLAGS(windows + i).do_iconify_window_groups = 1;
		windows[i].scratch.i = 17 + i;
	}
	mark_transient_subtree(windows + 7, MARK_ALL_LAYERS, MARK_ALL,
		False, True);
	for (unsigned i = 0; i < 8; i++)
	{
		assert(IS_IN_TRANSIENT_SUBTREE(windows + i));
		assert(windows[i].scratch.i == 17 + i);
		XDeleteContext(dpy, FW_W(windows + i), FvwmContext);
	}
	Scr.FvwmRoot.next = NULL;
	Scr.FvwmRoot.stack_next = Scr.FvwmRoot.stack_prev = &Scr.FvwmRoot;
	puts("window-group-marks-retain-order-policy: passed");
}

static void test_identity_boundaries(void)
{
	FvwmWindow windows[3] = { 0 };

	setup(windows, 3, 0);
	FvwmWindow *child = windows + 2;
	Window client = FW_W(windows + 1);
	FW_W_TRANSIENTFOR(child) = None;
	assert(get_transientfor_fvwmwindow(child) == NULL);
	FW_W_TRANSIENTFOR(child) = Scr.Root;
	assert(get_transientfor_fvwmwindow(child) == NULL);
	FW_W_TRANSIENTFOR(child) = FW_W(child);
	assert(get_transientfor_fvwmwindow(child) == NULL);
	Window alias = 0x60000000UL;
	XSaveContext(dpy, alias, FvwmContext, (char *)(windows + 1));
	FW_W_TRANSIENTFOR(child) = alias;
	assert(get_transientfor_fvwmwindow(child) == NULL);
	XDeleteContext(dpy, alias, FvwmContext);
	FW_W_TRANSIENTFOR(child) = client;
	XDeleteContext(dpy, client, FvwmContext);
	assert(get_transientfor_fvwmwindow(child) == windows + 1);
	XSaveContext(dpy, client, FvwmContext, (char *)(windows + 1));
	/* Preserve the context briefly after list retirement, as destroy does. */
	windows[0].next = child;
	child->prev = windows;
	windows[1].prev = windows[1].next = NULL;
	assert(get_transientfor_fvwmwindow(child) == NULL);
	/* A reused client XID can exist on the list before its context replaces
	 * the retired record. The fallback must still find the new owner. */
	FW_W(windows) = client;
	assert(get_transientfor_fvwmwindow(child) == windows);
	for (unsigned i = 0; i < 3; i++)
	{
		XDeleteContext(dpy, 0x70000000UL + i, FvwmContext);
	}
	Scr.FvwmRoot.next = NULL;
	Scr.FvwmRoot.stack_next = Scr.FvwmRoot.stack_prev = &Scr.FvwmRoot;
	puts("identity-boundaries: passed");
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	test_identity_boundaries();
	test_cached_layer_marks();
	test_window_groups();
	for (unsigned n = 128; n <= 2048; n *= 2)
	{
		for (unsigned shape = 0; shape < 3; shape++)
		{
			test_closure(n, shape, False, False);
			if (shape == 2)
			{
				test_closure(n, shape, True, False);
			}
		}
	}
	for (unsigned n = 128; n <= 2048; n *= 2)
	{
		for (unsigned shape = 0; shape < 3; shape++)
		{
			test_closure(n, shape, False, True);
			if (shape == 2)
			{
				test_closure(n, shape, True, True);
			}
		}
	}
	puts("group-free-window-hints-use-parent-worklist: passed");
	puts("ordered-transient-closure-ranks: passed");
	puts("transient-worklist-fallback: passed");
	fflush(stdout);
	_exit(0);
}
