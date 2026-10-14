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
#include "fvwm/add_window.h"

#define WINDOW_COUNT 128

static FvwmWindow windows[WINDOW_COUNT];
static unsigned steps;
static Bool active;
extern int __real_XFindContext(Display *, XID, XContext, XPointer *);

int __wrap_XFindContext(Display *display, XID window, XContext context,
	XPointer *data)
{
	if (active && context == FvwmContext)
	{
		/* Finite regression failure even if the guard is removed. */
		assert(++steps < 4096);
	}
	return __real_XFindContext(display, window, context, data);
}

static void reset_chain(void)
{
	steps = 0;
	for (unsigned index = 0; index < WINDOW_COUNT; index++)
	{
		SET_TRANSIENT(windows + index, 1);
		windows[index].flags.common.s.do_stack_transient_parent = 1;
		windows[index].layer = 4 + index;
		FW_W_TRANSIENTFOR(windows + index) = None;
	}
}

static void test_cycle(unsigned prefix, unsigned length)
{
	reset_chain();
	for (unsigned index = 0; index < prefix + length; index++)
	{
		unsigned parent = index + 1 == prefix + length ? prefix :
			index + 1;
		FW_W_TRANSIENTFOR(windows + index) = FW_W(windows + parent);
	}
	new_layer(windows, windows[0].layer);
	assert(steps > 0 && steps < 4 * (prefix + length));
	for (unsigned index = 0; index < prefix + length; index++)
	{
		assert(IS_TRANSIENT(windows + index));
		assert(windows[index].layer == 4 + index);
		assert(FW_W_TRANSIENTFOR(windows + index) ==
			FW_W(windows + (index + 1 == prefix + length ? prefix :
			index + 1)));
	}
}

static void test_hint_validation(void)
{
	reset_chain();
	FW_W_TRANSIENTFOR(windows) = FW_W(windows + 1);
	FW_W_TRANSIENTFOR(windows + 1) = FW_W(windows + 2);
	FW_W_TRANSIENTFOR(windows + 2) = FW_W(windows + 1);
	assert(validate_transientfor(windows) == False);
	assert(FW_W_TRANSIENTFOR(windows) == Scr.Root);
	assert(steps == 4);
	assert(FW_W_TRANSIENTFOR(windows + 1) == FW_W(windows + 2));
	assert(FW_W_TRANSIENTFOR(windows + 2) == FW_W(windows + 1));

	reset_chain();
	for (unsigned index = 0; index + 1 < WINDOW_COUNT; index++)
	{
		FW_W_TRANSIENTFOR(windows + index) = FW_W(windows + index + 1);
	}
	assert(validate_transientfor(windows) == True);
	assert(steps == WINDOW_COUNT);
	assert(FW_W_TRANSIENTFOR(windows) == FW_W(windows + 1));

	reset_chain();
	FW_W_TRANSIENTFOR(windows) = FW_W(windows + 1);
	FW_W_TRANSIENTFOR(windows + 1) = FW_W(windows);
	assert(validate_transientfor(windows) == False);
	assert(FW_W_TRANSIENTFOR(windows) == Scr.Root && steps == 2);
	puts("hint-cycle-rejection-and-acyclic-lookup-counts: passed");
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	assert(Scr.FvwmRoot.next == NULL);
	Scr.FvwmRoot.next = windows;
	for (unsigned index = 0; index < WINDOW_COUNT; index++)
	{
		FW_W(windows + index) = 0x70000000UL + index;
		windows[index].prev = index == 0 ? &Scr.FvwmRoot :
			windows + index - 1;
		windows[index].next = index + 1 == WINDOW_COUNT ? NULL :
			windows + index + 1;
		assert(XSaveContext(dpy, FW_W(windows + index), FvwmContext,
			(char *)(windows + index)) == 0);
	}
	active = True;
	test_cycle(0, 2);
	assert(steps == 3);
	test_cycle(0, 3);
	test_cycle(7, 2);
	test_cycle(17, 31);
	puts("cyclic-parent-original-target-and-unchanged-hints: passed");

	reset_chain();
	for (unsigned index = 0; index + 1 < WINDOW_COUNT; index++)
	{
		FW_W_TRANSIENTFOR(windows + index) = FW_W(windows + index + 1);
	}
	new_layer(windows, windows[WINDOW_COUNT - 1].layer);
	assert(steps == WINDOW_COUNT - 1);
	puts("deep-acyclic-ancestor-and-single-lookup-per-step: passed");

	Window terminals[] = { None, Scr.Root, FW_W(windows), 0x60000000UL };
	for (unsigned index = 0; index < 4; index++)
	{
		reset_chain();
		FW_W_TRANSIENTFOR(windows) = terminals[index];
		new_layer(windows, windows[0].layer);
		assert(steps <= 1);
	}
	reset_chain();
	FW_W_TRANSIENTFOR(windows) = FW_W(windows + 1);
	windows[0].flags.common.s.do_stack_transient_parent = 0;
	new_layer(windows, windows[0].layer);
	assert(steps == 0);
	puts("root-self-missing-and-style-stop: passed");
	test_hint_validation();

	reset_chain();
	FW_W_TRANSIENTFOR(windows) = FW_W(windows + WINDOW_COUNT - 1);
	windows[WINDOW_COUNT - 2].next = NULL;
	windows[WINDOW_COUNT - 1].prev = NULL;
	new_layer(windows, windows[0].layer);
	assert(steps == 1);
	puts("retired-parent-not-selected: passed");
	fflush(stdout);
	_exit(0);
}
