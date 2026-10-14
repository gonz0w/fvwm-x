/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include "fvwm/borders.h"
#include "libs/FScreen.h"

static FvwmWindow windows[4];
static Bool active;
static unsigned calls, invalid, submitted;
static Bool fail_storage;
static Bool refusal_seen;
void *__real_malloc(size_t size);

void *__wrap_malloc(size_t size)
{
	unsigned icons = getenv("FVWM_COUNT_ICONS") ?
		(unsigned)atoi(getenv("FVWM_COUNT_ICONS")) : 2;

	if (fail_storage && size == (7 + icons) * sizeof(Window))
	{
		fail_storage = False;
		refusal_seen = True;
		errno = ENOMEM;
		return NULL;
	}
	return __real_malloc(size);
}

int __real_XRestackWindows(Display *display, Window *list, int count);
static unsigned publications;

int __wrap_XRestackWindows(Display *display, Window *list, int count)
{
	if (!active)
	{
		return __real_XRestackWindows(display, list, count);
	}
	calls++;
	submitted = count;
	for (int i = 0; i < count; i++)
	{
		if (list[i] == None)
		{
			invalid++;
		}
	}
	printf("restack slots: %d; empty allocation slots sent: %u\n", count, invalid);
	assert(invalid == 0);
	return __real_XRestackWindows(display, list, count);
}

void __wrap_focus_grab_buttons_on_layer(int layer)
{
	publications += active;
}

void __wrap_EWMH_SetWMState(FvwmWindow *window, Bool restore)
{
	publications += active;
}

void __wrap_border_draw_decorations(
	FvwmWindow *window, window_parts draw_parts, Bool has_focus,
	int force, clear_window_parts clear_parts, rectangle *frame_g,
	rectangle *icon_g)
{
	publications += active;
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	unsigned icons = getenv("FVWM_COUNT_ICONS") ?
		(unsigned)atoi(getenv("FVWM_COUNT_ICONS")) : 2;
	Window frames[4];
	FvwmWindow originals[4];
	int target_layer = getenv("FVWM_LAYER_LOWER") ? 3 : 6;

	assert(icons <= 2);
	Scr.FvwmRoot.next = windows;
	Scr.FvwmRoot.stack_next = windows;
	Scr.FvwmRoot.stack_prev = windows + 3;
	Scr.FvwmRoot.scratch.i = 0;
	for (unsigned i = 0; i < 4; i++)
	{
		FvwmWindow *w = windows + i;

		w->next = i == 3 ? NULL : w + 1;
		w->prev = i == 0 ? &Scr.FvwmRoot : w - 1;
		w->stack_next = i == 3 ? &Scr.FvwmRoot : w + 1;
		w->stack_prev = i == 0 ? &Scr.FvwmRoot : w - 1;
		w->layer = 4;
		w->m = monitor_get_current();
		assert(w->m);
		w->Desk = w->m->virtual_scr.CurrentDesk;
		FW_W(w) = XCreateSimpleWindow(dpy, Scr.Root, 0, 0, 10, 10, 0, 0, 0);
		FW_W_FRAME(w) = FW_W(w);
		FW_COMMON_STATIC_FLAGS(w).do_raise_transient = 1;
		FW_COMMON_STATIC_FLAGS(w).do_lower_transient = 1;
		frames[i] = FW_W(w);
		assert(XSaveContext(dpy, FW_W(w), FvwmContext, (char *)w) == 0);
	}
	SET_ICONIFIED(windows, 1);
	if (icons != 0)
	{
		FW_W_ICON_TITLE(windows) = XCreateSimpleWindow(
			dpy, Scr.Root, 0, 0, 10, 10, 0, 0, 0);
	}
	if (icons == 2)
	{
		FW_W_ICON_PIXMAP(windows) = XCreateSimpleWindow(
			dpy, Scr.Root, 0, 0, 10, 10, 0, 0, 0);
	}
	SET_TRANSIENT(windows + 1, 1);
	SET_TRANSIENT(windows + 2, 1);
	FW_W_TRANSIENTFOR(windows + 1) = FW_W(windows);
	FW_W_TRANSIENTFOR(windows + 2) = FW_W(windows + 1);
	XRestackWindows(dpy, frames, 4);
	XSync(dpy, False);
	memcpy(originals, windows, sizeof(windows));
	active = True;
	fail_storage = getenv("FVWM_LAYER_REFUSE") != NULL;
	new_layer(windows, target_layer);
	active = False;
	assert(refusal_seen == (getenv("FVWM_LAYER_REFUSE") != NULL));
	if (refusal_seen)
	{
		assert(calls == 0 && publications == 0);
		assert(memcmp(originals, windows, sizeof(windows)) == 0);
		FvwmWindow *current = Scr.FvwmRoot.stack_next;

		for (unsigned i = 0; i < 4; i++)
		{
			assert(current == windows + i);
			assert(windows[i].layer == 4);
			assert(windows[i].scratch.i == 0);
			assert(!IS_IN_TRANSIENT_SUBTREE(windows + i));
			assert(windows[i].scratch.p == NULL);
			assert(current->stack_prev->stack_next == current);
			assert(current->stack_next->stack_prev == current);
			current = current->stack_next;
		}
		assert(current == &Scr.FvwmRoot);
		Window root, parent, *children;
		unsigned child_count, position = 0;

		assert(XQueryTree(dpy, Scr.Root, &root, &parent, &children, &child_count));
		for (unsigned i = child_count; i-- > 0;)
		{
			for (unsigned j = 0; j < 4; j++)
			{
				if (children[i] == frames[j])
				{
					assert(position < 4 && frames[position++] == children[i]);
					break;
				}
			}
		}
		assert(position == 4);
		XFree(children);
		puts("layer-storage-refusal-preserves-public-and-scratch-state: passed");
		/* Prove failure doesn't poison the following ordinary operation. */
		active = False;
		new_layer(windows, target_layer);
		for (unsigned i = 0; i < 3; i++)
		{
			assert(windows[i].layer == target_layer);
		}
		assert(windows[3].layer == 4);
		puts("layer-storage-refusal-retry: passed");
	}
	else
	{
		assert(!fail_storage);
		assert(calls == 1 && invalid == 0 && submitted == 3 + icons);
		for (unsigned i = 0; i < 3; i++)
		{
			assert(windows[i].layer == target_layer);
		}
		assert(windows[3].layer == 4);
		puts("ordinary-layer-control: passed");
	}
	printf("icon-count-%u-control: finished; invalid=%u\n", icons, invalid);
	fflush(stdout);
	_exit(0);
}
