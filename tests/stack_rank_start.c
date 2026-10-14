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

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	const unsigned count = 12000;
	FvwmWindow *windows = calloc(count, sizeof(*windows));

	assert(windows);
	setup(windows, count, 0);
	const int64_t bonuses[] = { 200000, -2000000000 };

	for (unsigned mode = 0; mode < 2; mode++)
	{
		for (unsigned i = 0; i < count; i++)
		{
			windows[i].scratch.i = i == 0 ? 0 : bonuses[mode];
		}
		mark_transient_subtree(windows, MARK_ALL_LAYERS, MARK_ALL,
			False, False);
		for (unsigned i = 0; i < count; i++)
		{
			assert(IS_IN_TRANSIENT_SUBTREE(windows + i));
			assert(windows[i].scratch.i ==
				(int64_t)i * (bonuses[mode] + 1));
		}
	}
	for (unsigned i = 0; i < count; i++)
	{
		XDeleteContext(dpy, FW_W(windows + i), FvwmContext);
	}
	puts("deep-transient-ranks-retain-branch-weights: passed");
	free(windows);
	windows = calloc(24, sizeof(*windows));
	assert(windows);
	setup(windows, 24, 0);
	for (unsigned iteration = 0; iteration < 128; iteration++)
	{
		mark_transient_subtree(windows, MARK_ALL_LAYERS, MARK_ALL,
			False, False);
		for (unsigned i = 0; i < 24; i++)
		{
			assert(IS_IN_TRANSIENT_SUBTREE(windows + i));
			assert(windows[i].scratch.i >= 0);
		}
	}
	assert(windows[23].scratch.i == INT64_MAX);
	for (unsigned i = 0; i < 24; i++)
	{
		XDeleteContext(dpy, FW_W(windows + i), FvwmContext);
	}
	free(windows);
	puts("repeated-marking-finishes-with-bounded-ranks: passed");
	fflush(stdout);
	_exit(0);
}
