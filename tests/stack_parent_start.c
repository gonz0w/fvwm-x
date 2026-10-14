/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "fvwm/stack.h"
#include "libs/FScreen.h"

/* Synthetic managed records use real unmapped X frames. Existing real-client
 * drivers separately cover lifetime, layer/group hints and window retirement. */
void __wrap_focus_grab_buttons_on_layer(int layer)
{
}

#define PARITY_COUNT 128
static FvwmWindow parity_windows[PARITY_COUNT];

static void reset_parity_windows(unsigned prefix, unsigned length, Bool groups)
{
	Window frames[PARITY_COUNT];

	Scr.FvwmRoot.scratch.i = 0;
	Scr.FvwmRoot.next = parity_windows;
	Scr.FvwmRoot.stack_next = parity_windows + PARITY_COUNT - 1;
	Scr.FvwmRoot.stack_prev = parity_windows;
	for (unsigned i = 0; i < PARITY_COUNT; i++)
	{
		parity_windows[i].prev = i == 0 ? &Scr.FvwmRoot : parity_windows + i - 1;
		parity_windows[i].next = i + 1 == PARITY_COUNT ? NULL : parity_windows + i + 1;
		parity_windows[i].stack_prev = i + 1 == PARITY_COUNT ? &Scr.FvwmRoot :
			parity_windows + i + 1;
		parity_windows[i].stack_next = i == 0 ? &Scr.FvwmRoot : parity_windows + i - 1;
		SET_IN_TRANSIENT_SUBTREE(parity_windows + i, 0);
		parity_windows[i].scratch.p = NULL;
		SET_TRANSIENT(parity_windows + i, i < prefix + length);
		parity_windows[i].flags.common.s.do_stack_transient_parent = 1;
		parity_windows[i].flags.common.s.do_raise_transient = groups;
		parity_windows[i].flags.common.s.do_lower_transient = groups;
		FW_W_TRANSIENTFOR(parity_windows + i) = None;
		frames[PARITY_COUNT - 1 - i] = FW_W_FRAME(parity_windows + i);
	}
	for (unsigned i = 0; i < prefix + length; i++)
	{
		FW_W_TRANSIENTFOR(parity_windows + i) = FW_W(parity_windows +
			(i + 1 == prefix + length ? prefix : i + 1));
	}
	XRestackWindows(dpy, frames, PARITY_COUNT);
}

static void observe_ring(Window order[PARITY_COUNT])
{
	FvwmWindow *current = Scr.FvwmRoot.stack_next;

	for (unsigned i = 0; i < PARITY_COUNT; i++)
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
		for (unsigned j = 0; j < PARITY_COUNT; j++)
		{
			if (children[i] == FW_W_FRAME(parity_windows + j))
			{
				assert(position < PARITY_COUNT &&
					order[position++] == FW_W(parity_windows + j));
			}
		}
	}
	XFree(children);
	assert(position == PARITY_COUNT);
}

static void check_parity(void)
{
	void (*operations[])(FvwmWindow *, Bool) = {
		RaiseWindow, LowerWindow, RestackWindow};
	for (unsigned i = 0; i < PARITY_COUNT; i++)
	{
		FW_W(parity_windows + i) = XCreateSimpleWindow(dpy, Scr.Root,
			0, 0, 10, 10, 0, 0, 0);
		FW_W_FRAME(parity_windows + i) = FW_W(parity_windows + i);
		parity_windows[i].m = monitor_get_current();
		assert(parity_windows[i].m != NULL);
		parity_windows[i].Desk = parity_windows[i].m->virtual_scr.CurrentDesk;
		parity_windows[i].layer = 4;
	}
	for (unsigned prefix = 0; prefix < 6; prefix += prefix == 0 ? 1 : 4)
	{
		const unsigned lengths[] = { 2, 3, 40, 64, 100 };
		for (unsigned li = 0; li < 5; li++)
		{
			unsigned length = lengths[li];
			for (unsigned groups = 0; groups < 2; groups++)
			{
				for (unsigned op = 0; op < 3; op++)
				{
					Window actual[PARITY_COUNT], direct[PARITY_COUNT];
					reset_parity_windows(prefix, length, groups);
					operations[op](parity_windows, False);
					observe_ring(actual);
					for (unsigned i = 0; i < prefix + length; i++)
					{
						assert(IS_TRANSIENT(parity_windows + i));
						assert(FW_W_TRANSIENTFOR(parity_windows + i) ==
							FW_W(parity_windows + (i + 1 == prefix + length ?
							prefix : i + 1)));
					}
					reset_parity_windows(prefix, length, groups);
					parity_windows[0].flags.common.s.do_stack_transient_parent = 0;
					operations[op](parity_windows, False);
					observe_ring(direct);
					for (unsigned i = 0; i < PARITY_COUNT; i++)
					{
						assert(actual[i] == direct[i]);
					}
				}
			}
		}
	}
	puts("cyclic-redirection-matches-direct-target-and-transient-rules: passed");
	reset_parity_windows(0, 2, False);
	SET_TRANSIENT(parity_windows + 1, 0);
	RaiseWindow(parity_windows, False);
	assert(Scr.FvwmRoot.stack_next == parity_windows);
	puts("acyclic-redirection-and-following-operation: passed");
	fflush(stdout);
	_exit(0);
}

#define FAILURE_COUNT 66
static FvwmWindow failure_windows[FAILURE_COUNT];
static Bool fail_index;

void *__real_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size)
{
	if (fail_index)
	{
		fail_index = False;
		return NULL;
	}
	return __real_calloc(count, size);
}

static void check_storage(void)
{
	Bool lower = getenv("FVWM_PARENT_LOWER") != NULL;
	Bool restack = getenv("FVWM_PARENT_RESTACK") != NULL;
	Bool refuse = getenv("FVWM_PARENT_FAIL") != NULL;
	Window frames[FAILURE_COUNT];
	unsigned order[FAILURE_COUNT];

	Scr.FvwmRoot.next = failure_windows;
	Scr.FvwmRoot.scratch.i = 0;
	Scr.bo.do_raise_over_unmanaged = False;
	Scr.bo.is_raise_hack_needed = False;
	for (unsigned i = 0; i < FAILURE_COUNT; i++)
	{
		FvwmWindow *w = failure_windows + i;
		w->prev = i ? w - 1 : &Scr.FvwmRoot;
		w->next = i + 1 == FAILURE_COUNT ? NULL : w + 1;
		FW_W(w) = XCreateSimpleWindow(dpy, Scr.Root,
			0, 0, 10, 10, 0, 0, 0);
		FW_W_FRAME(w) = FW_W(w);
		w->m = monitor_get_current();
		assert(w->m);
		w->Desk = w->m->virtual_scr.CurrentDesk;
		w->layer = 4;
		FW_COMMON_STATIC_FLAGS(w).do_stack_transient_parent = 1;
		FW_COMMON_STATIC_FLAGS(w).do_raise_transient = 1;
		FW_COMMON_STATIC_FLAGS(w).do_lower_transient = 1;
		assert(XSaveContext(dpy, FW_W(w), FvwmContext, (char *)w) == 0);
	}
	for (unsigned i = 1; i + 1 < FAILURE_COUNT; i++)
	{
		SET_TRANSIENT(failure_windows + i, 1);
		FW_W_TRANSIENTFOR(failure_windows + i) = FW_W(failure_windows + i + 1);
	}
	for (unsigned i = 0; i < FAILURE_COUNT; i++)
	{
		order[i] = lower ? (i + 1) % FAILURE_COUNT : i;
	}
	Scr.FvwmRoot.stack_next = failure_windows + order[0];
	Scr.FvwmRoot.stack_prev = failure_windows + order[FAILURE_COUNT - 1];
	for (unsigned i = 0; i < FAILURE_COUNT; i++)
	{
		FvwmWindow *w = failure_windows + order[i];
		w->stack_prev = i ? failure_windows + order[i - 1] : &Scr.FvwmRoot;
		w->stack_next = i + 1 == FAILURE_COUNT ? &Scr.FvwmRoot :
			failure_windows + order[i + 1];
		frames[i] = FW_W_FRAME(w);
	}
	XRestackWindows(dpy, frames, FAILURE_COUNT);
	XSync(dpy, False);
	fail_index = refuse;
	if (restack)
	{
		RestackWindow(failure_windows + 1, False);
	}
	else if (lower)
	{
		LowerWindow(failure_windows + 1, False);
	}
	else
	{
		RaiseWindow(failure_windows + 1, False);
	}
	assert(!fail_index);
	FvwmWindow *current = Scr.FvwmRoot.stack_next;
	for (unsigned i = 0; i < FAILURE_COUNT; i++)
	{
		unsigned expected = restack ? i : lower ? (refuse ?
			(i < FAILURE_COUNT - 2 ? i + 2 : i == FAILURE_COUNT - 2 ? 0 : 1) : i) :
			(refuse ? (i == 0 ? 1 : i == 1 ? 0 : i) : (i + 1) % FAILURE_COUNT);
		assert(current == failure_windows + expected);
		assert(current->stack_prev->stack_next == current);
		frames[i] = FW_W_FRAME(current);
		current = current->stack_next;
	}
	assert(current == &Scr.FvwmRoot);
	Window root, parent, *children;
	unsigned count, position = 0;
	assert(XQueryTree(dpy, Scr.Root, &root, &parent, &children, &count));
	for (unsigned i = count; i-- > 0;)
	{
		for (unsigned j = 0; j < FAILURE_COUNT; j++)
		{
			if (children[i] == FW_W_FRAME(failure_windows + j))
			{
				assert(position < FAILURE_COUNT &&
					frames[position++] == children[i]);
				break;
			}
		}
	}
	assert(position == FAILURE_COUNT);
	XFree(children);
	printf("%s-%s-exact-ring-and-server-order: passed\n",
		restack ? "restack" : lower ? "lower" : "raise", refuse ? "invoked-fallback" : "ancestry");
	fflush(stdout);
	_exit(0);
}

static void check_depth(void)
{
	unsigned count = 4096;
	FvwmWindow *windows = calloc(count, sizeof(*windows));
	Window *frames = calloc(count, sizeof(*frames));

	assert(windows && frames);
	Scr.FvwmRoot.next = windows;
	Scr.FvwmRoot.stack_next = windows;
	Scr.FvwmRoot.stack_prev = windows + count - 1;
	Scr.FvwmRoot.scratch.i = 0;
	Scr.bo.do_raise_over_unmanaged = False;
	Scr.bo.is_raise_hack_needed = False;
	for (unsigned i = 0; i < count; i++)
	{
		FvwmWindow *w = windows + i;
		w->prev = i ? w - 1 : &Scr.FvwmRoot;
		w->next = i + 1 == count ? NULL : w + 1;
		w->stack_prev = i ? w - 1 : &Scr.FvwmRoot;
		w->stack_next = i + 1 == count ? &Scr.FvwmRoot : w + 1;
		FW_W(w) = XCreateSimpleWindow(dpy, Scr.Root,
			0, 0, 10, 10, 0, 0, 0);
		FW_W_FRAME(w) = FW_W(w);
		w->m = monitor_get_current();
		assert(w->m);
		w->Desk = w->m->virtual_scr.CurrentDesk;
		w->layer = 4;
		FW_COMMON_STATIC_FLAGS(w).do_stack_transient_parent = 1;
		FW_COMMON_STATIC_FLAGS(w).do_raise_transient = 1;
		FW_COMMON_STATIC_FLAGS(w).do_lower_transient = 1;
		assert(XSaveContext(dpy, FW_W(w), FvwmContext, (char *)w) == 0);
		frames[i] = FW_W_FRAME(w);
	}
	for (unsigned i = 0; i + 1 < count; i++)
	{
		SET_TRANSIENT(windows + i, 1);
		FW_W_TRANSIENTFOR(windows + i) = FW_W(windows + i + 1);
	}
	XRestackWindows(dpy, frames, count);
	XSync(dpy, False);
	if (getenv("FVWM_DEPTH_SMALL_STACK"))
	{
		struct rlimit limit;
		assert(getrlimit(RLIMIT_STACK, &limit) == 0);
		limit.rlim_cur = 128 * 1024;
		assert(setrlimit(RLIMIT_STACK, &limit) == 0);
		printf("starting finite %u-parent Raise with 128-KiB stack limit\n", count);
	}
	else
	{
		printf("starting finite %u-parent Raise with default stack limit\n", count);
	}
	fflush(stdout);
	if (getenv("FVWM_PARENT_LOWER"))
	{
		LowerWindow(windows, False);
	}
	else if (getenv("FVWM_PARENT_RESTACK"))
	{
		RestackWindow(windows, False);
	}
	else
	{
		RaiseWindow(windows, False);
	}
	FvwmWindow *current = Scr.FvwmRoot.stack_next;
	for (unsigned i = 0; i < count; i++)
	{
		assert(current != &Scr.FvwmRoot);
		assert(current->stack_next->stack_prev == current);
		assert(current->stack_prev->stack_next == current);
		frames[i] = FW_W_FRAME(current);
		current = current->stack_next;
		if (i + 1 < count)
		{
			assert(IS_TRANSIENT(windows + i));
			assert(FW_W_TRANSIENTFOR(windows + i) == FW_W(windows + i + 1));
		}
	}
	assert(current == &Scr.FvwmRoot);
	Window root, parent, *children;
	unsigned child_count, position = 0;
	assert(XQueryTree(dpy, Scr.Root, &root, &parent, &children, &child_count));
	for (unsigned i = child_count; i-- > 0;)
	{
		FvwmWindow *record = NULL;

		if (XFindContext(dpy, children[i], FvwmContext,
			(XPointer *)&record) == 0)
		{
			assert(position < count && frames[position++] == children[i]);
		}
	}
	assert(position == count);
	XFree(children);
	puts("finite-deep-parent-operation: passed");
	fflush(stdout);
	_exit(0);
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	if (getenv("FVWM_PARENT_DEPTH"))
	{
		check_depth();
	}
	else if (getenv("FVWM_PARENT_STORAGE"))
	{
		check_storage();
	}
	else
	{
		check_parity();
	}
	assert(0);
}
