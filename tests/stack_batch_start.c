/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "fvwm/stack.h"
#include "fvwm/stack_window_list.h"
#include "libs/monitor_types.h"

static Bool active;
static unsigned calls, edges, resizes;
void *__real_fxrealloc(void *pointer, size_t count, size_t size);
int __real_XRestackWindows(Display *display, Window *windows, int count);

void *__wrap_fxrealloc(void *pointer, size_t count, size_t size)
{
	resizes += active;
	return __real_fxrealloc(pointer, count, size);
}

int __wrap_XRestackWindows(Display *display, Window *windows, int count)
{
	if (active)
	{
		assert(count > 1 && count <= FVWM_STACK_WINDOW_BATCH_CAPACITY);
		for (int i = 0; i < count; i++)
		{
			assert(windows[i] != None);
		}
		calls++;
		edges += count - 1;
	}
	return __real_XRestackWindows(display, windows, count);
}

void __wrap_focus_grab_buttons_on_layer(int layer)
{
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	unsigned count = (unsigned)atoi(getenv("FVWM_BATCH_COUNT"));
	unsigned mode = (unsigned)atoi(getenv("FVWM_BATCH_MODE"));
	FvwmWindow *windows = calloc(count, sizeof(*windows));
	Window *expected = calloc(count * 3, sizeof(*expected));
	size_t used = 0;
	Bool new_window = mode >= 3;

	assert(windows != NULL && expected != NULL && count >= 3 && count <= 130);
	assert(mode < 5);
	Scr.FvwmRoot.next = windows;
	Scr.FvwmRoot.stack_next = windows;
	Scr.FvwmRoot.stack_prev = windows + count - 1;
	for (unsigned i = 0; i < count; i++)
	{
		FvwmWindow *window = windows + i;

		window->prev = i == 0 ? &Scr.FvwmRoot : window - 1;
		window->next = i + 1 == count ? NULL : window + 1;
		window->stack_prev = i == 0 ? &Scr.FvwmRoot : window - 1;
		window->stack_next = i + 1 == count ? &Scr.FvwmRoot : window + 1;
		window->layer = 4;
		window->m = monitor_get_current();
		assert(window->m != NULL);
		window->Desk = window->m->virtual_scr.CurrentDesk;
		FW_W(window) = XCreateSimpleWindow(
			dpy, Scr.Root, 0, 0, 10, 10, 0, 0, 0);
		FW_W_FRAME(window) = FW_W(window);
		FW_COMMON_STATIC_FLAGS(window).do_raise_transient = 1;
		FW_COMMON_STATIC_FLAGS(window).do_lower_transient = 1;
		assert(XSaveContext(dpy, FW_W(window), FvwmContext, (char *)window) == 0);
		if (i >= 2)
		{
			SET_TRANSIENT(window, 1);
			FW_W_TRANSIENTFOR(window) = FW_W(windows + (new_window ? 1 : 0));
			if (i >= 3 && i % 8 == 2)
			{
				SET_ICONIFIED(window, 1);
				SET_ICON_SUPPRESSED(window, i % 16 == 2);
				FW_W_ICON_TITLE(window) = XCreateSimpleWindow(
					dpy, Scr.Root, 0, 0, 10, 10, 0, 0, 0);
				FW_W_ICON_PIXMAP(window) = XCreateSimpleWindow(
					dpy, Scr.Root, 0, 0, 10, 10, 0, 0, 0);
			}
		}
		assert(fvwm_stack_window_append(expected, count * 3, &used, window));
	}
	if (new_window)
	{
		SET_TRANSIENT(windows, 1);
		FW_W_TRANSIENTFOR(windows) = FW_W(windows + 1);
		FW_COMMON_STATIC_FLAGS(windows).do_stack_transient_parent = 1;
	}
	XRestackWindows(dpy, expected, (int)used);
	XSync(dpy, False);
	active = True;
	switch (mode)
	{
	case 0:
		RaiseWindow(windows, False);
		break;
	case 1:
		LowerWindow(windows, False);
		break;
	case 2:
		RestackWindow(windows, False);
		break;
	default:
		assert(position_new_window_in_stack_ring(windows, mode == 4));
		break;
	}
	active = False;
	assert(resizes == 0 && calls > 0);
	if (count == 130)
	{
		assert(calls > 1 && edges > FVWM_STACK_WINDOW_BATCH_CAPACITY);
	}
	used = 0;
	unsigned members = 0;
	for (FvwmWindow *window = Scr.FvwmRoot.stack_next;
	     window != &Scr.FvwmRoot; window = window->stack_next)
	{
		assert(members++ < count);
		assert(window->stack_next->stack_prev == window);
		assert(window->stack_prev->stack_next == window);
		assert(fvwm_stack_window_append(expected, count * 3, &used, window));
	}
	assert(members == count);
	Window root, parent, *children;
	unsigned child_count;
	size_t position = 0;

	assert(XQueryTree(dpy, Scr.Root, &root, &parent, &children, &child_count));
	for (unsigned i = child_count; i-- > 0;)
	{
		for (size_t j = 0; j < used; j++)
		{
			if (children[i] == expected[j])
			{
				if (position >= used || expected[position] != children[i])
				{
					fprintf(stderr, "stack mismatch at %zu: expected 0x%lx "
						"observed 0x%lx\n", position,
						position < used ? expected[position] : None,
						children[i]);
				}
				assert(position < used && expected[position++] == children[i]);
				break;
			}
		}
	}
	assert(position == used);
	XFree(children);
	free(expected);
	free(windows);
	printf("bounded-restack-and-server-ring-order: passed; batches=%u "
	       "edges=%u resizes=%u\n", calls, edges, resizes);
	fflush(stdout);
	_exit(0);
}
