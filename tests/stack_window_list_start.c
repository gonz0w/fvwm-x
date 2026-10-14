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
#include "fvwm/borders.h"
#include "libs/FScreen.h"

static FvwmWindow windows[4];
static Bool active;
static unsigned calls, invalid, submitted;
static Bool suppressed;
void *__real_fxmalloc(size_t size);
int __real_XRestackWindows(Display *display, Window *list, int count);

void *__wrap_fxmalloc(size_t size)
{
	void *result = __real_fxmalloc(size);

	if (active)
	{
		/* Deterministic control: unused allocation slots contain None. */
		memset(result, 0, size);
	}
	return result;
}

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
		Bool known = False;

		for (unsigned j = 0; j < 4; j++)
		{
			if (IS_SCHEDULED_FOR_DESTROY(windows + j))
			{
				continue;
			}
			known |= list[i] == FW_W_FRAME(windows + j);
			if (!suppressed)
			{
				known |= list[i] != None &&
					(list[i] == FW_W_ICON_TITLE(windows + j) ||
					 list[i] == FW_W_ICON_PIXMAP(windows + j));
			}
		}
		if (!known)
		{
			invalid++;
		}
	}
	printf("restack slots: %d; empty allocation slots sent: %u\n", count, invalid);
	/* No malformed test requests reach Xlib. Invalid IDs fail below. */
	return invalid ? 1 : __real_XRestackWindows(display, list, count);
}

void __wrap_focus_grab_buttons_on_layer(int layer)
{
}

void __wrap_EWMH_SetWMState(FvwmWindow *window, Bool restore)
{
}

void __wrap_border_draw_decorations(
	FvwmWindow *window, window_parts draw_parts, Bool has_focus,
	int force, clear_window_parts clear_parts, rectangle *frame_g,
	rectangle *icon_g)
{
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	unsigned icons = getenv("FVWM_COUNT_ICONS") ?
		(unsigned)atoi(getenv("FVWM_COUNT_ICONS")) : 2;
	Window frames[4];

	Bool new_window = getenv("FVWM_COUNT_NEW") != NULL;
	Bool detached = getenv("FVWM_COUNT_DETACHED") != NULL;
	Bool lower = getenv("FVWM_COUNT_LOWER") != NULL;
	unsigned members = new_window ? 3 : 4;
	unsigned icon_owner = new_window ? 2 : 0;

	suppressed = getenv("FVWM_COUNT_SUPPRESSED") != NULL;
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
	SET_ICONIFIED(windows + icon_owner, 1);
	SET_ICON_SUPPRESSED(windows + icon_owner, suppressed);
	if (icons != 0)
	{
		FW_W_ICON_TITLE(windows + icon_owner) = XCreateSimpleWindow(
			dpy, Scr.Root, 0, 0, 10, 10, 0, 0, 0);
	}
	if (icons == 2)
	{
		FW_W_ICON_PIXMAP(windows + icon_owner) = XCreateSimpleWindow(
			dpy, Scr.Root, 0, 0, 10, 10, 0, 0, 0);
	}
	if (new_window)
	{
		windows[2].stack_next = &Scr.FvwmRoot;
		Scr.FvwmRoot.stack_prev = windows + 2;
		windows[2].next = detached ? windows + 3 : NULL;
		windows[3].stack_next = NULL;
		windows[3].stack_prev = NULL;
		SET_SCHEDULED_FOR_DESTROY(windows + 3, 1);
		SET_TRANSIENT(windows, 1);
		FW_COMMON_STATIC_FLAGS(windows).do_stack_transient_parent = 1;
		FW_W_TRANSIENTFOR(windows) = FW_W(windows + 1);
	}
	else
	{
		SET_TRANSIENT(windows + 1, 1);
		SET_TRANSIENT(windows + 2, 1);
		FW_W_TRANSIENTFOR(windows + 1) = FW_W(windows);
		FW_W_TRANSIENTFOR(windows + 2) = FW_W(windows + 1);
	}
	XRestackWindows(dpy, frames, 4);
	XSync(dpy, False);
	active = True;
	if (new_window)
	{
		assert(position_new_window_in_stack_ring(windows, lower));
	}
	else
	{
		new_layer(windows, lower ? 2 : 6);
	}
	active = False;
	assert(calls > 0 && invalid == 0);
	assert(submitted == 3 + (suppressed ? 0 : icons));
	if (!new_window)
	{
		for (unsigned i = 0; i < 3; i++)
		{
			assert(windows[i].layer == (lower ? 2 : 6));
		}
		assert(windows[3].layer == 4);
	}
	if (True)
	{
		Window expected[6];
		unsigned expected_count = 0;
		FvwmWindow *current = Scr.FvwmRoot.stack_next;

		for (unsigned i = 0; i < members; i++)
		{
			assert(current != &Scr.FvwmRoot);
			assert(current->stack_prev->stack_next == current);
			assert(current->stack_next->stack_prev == current);
			expected[expected_count++] = FW_W_FRAME(current);
			if (current == windows + icon_owner && !suppressed)
			{
				if (icons > 0)
				{
					expected[expected_count++] = FW_W_ICON_TITLE(current);
				}
				if (icons > 1)
				{
					expected[expected_count++] = FW_W_ICON_PIXMAP(current);
				}
			}
			current = current->stack_next;
		}
		assert(current == &Scr.FvwmRoot);
		Window root, parent, *children;
		unsigned count, position = 0;

		assert(XQueryTree(dpy, Scr.Root, &root, &parent, &children, &count));
		for (unsigned i = count; i-- > 0;)
		{
			for (unsigned j = 0; j < expected_count; j++)
			{
				if (children[i] == expected[j])
				{
					assert(position < expected_count &&
						expected[position++] == children[i]);
					break;
				}
			}
		}
		assert(position == expected_count);
		XFree(children);
	}
	puts("initialized-restack-and-server-ring-order: passed");
	fflush(stdout);
	_exit(0);
}
