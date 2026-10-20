/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/WinMagic.h"
#include "libs/wcontext.h"
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/misc.h"
#include "fvwm/screen.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	Window windows[65];
	int x = 7, y = 11;
	FvwmWindow record = {0};
	(void)exc;
	alarm(20);
	windows[0] = XCreateSimpleWindow(dpy, Scr.Root, 100, 100,
		200, 200, 0, 0, 0);
	for (unsigned i = 1; i < 65; i++)
		windows[i] = XCreateSimpleWindow(dpy, windows[i - 1],
			1, 1, 100, 100, 1, 0, 0);
	assert(GetTopAncestorWindow(dpy, windows[0]) == None);
	assert(GetTopAncestorWindow(dpy, windows[64]) == windows[0]);
	assert(FGetFrameWindow(dpy, windows[64], &x, &y) == windows[0]);
	assert(x == 135 && y == 139);
	puts("ancestor-and-frame-real-tree: passed");
	XMapWindow(dpy, windows[0]);
	XWarpPointer(dpy, None, windows[0], 0, 0, 0, 0, 10, 10);
	XSync(dpy, False);
	assert(get_pointer_fvwm_window() == NULL);
	assert(XSaveContext(dpy, windows[0], FvwmContext,
		(XPointer)&record) == 0);
	assert(get_pointer_fvwm_window() == &record);
	XDeleteContext(dpy, windows[0], FvwmContext);
	XWarpPointer(dpy, None, Scr.Root, 0, 0, 0, 0, 1, 1);
	XSync(dpy, False);
	assert(get_pointer_fvwm_window() == NULL);
	puts("pointer-context-and-unmanaged-real-tree: passed");
	/* The next query must observe the new parent, without a cached tree. */
	Window replacement = XCreateSimpleWindow(dpy, Scr.Root,
		300, 100, 200, 200, 0, 0, 0);
	XReparentWindow(dpy, windows[1], replacement, 2, 3);
	x = y = 0;
	assert(GetTopAncestorWindow(dpy, windows[64]) == replacement);
	assert(FGetFrameWindow(dpy, windows[64], &x, &y) == replacement);
	assert(x == 129 && y == 130);
	XDestroyWindow(dpy, windows[0]);
	XDestroyWindow(dpy, replacement);
	XSync(dpy, False);
	puts("reparent-refresh-real-tree: passed");
	fflush(stdout);
	_exit(0);
}
