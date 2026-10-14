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
#include "fvwm/add_window.h"
#include "fvwm/execcontext.h"
#include "fvwm/screen.h"
#include "fvwm/stack.h"
#include "fvwm/externs.h"

static unsigned context_lookups;
int __real_XFindContext(Display *display, XID xid, XContext context,
	XPointer *data);
int __wrap_XFindContext(Display *display, XID xid, XContext context,
	XPointer *data)
{
	context_lookups++;
	return __real_XFindContext(display, xid, context, data);
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	FvwmWindow windows[128] = { 0 };
	Window aliases[] = { 0x60000000UL, 0x60000001UL, 0x60000002UL };

	assert(Scr.FvwmRoot.next == NULL);
	Scr.FvwmRoot.next = windows;
	for (unsigned i = 0; i < 128; i++)
	{
		FW_W(windows + i) = 0x70000000UL + i;
		windows[i].prev = i == 0 ? &Scr.FvwmRoot : windows + i - 1;
		windows[i].next = i == 127 ? NULL : windows + i + 1;
		assert(XSaveContext(dpy, FW_W(windows + i), FvwmContext,
			(char *)(windows + i)) == 0);
	}
	FvwmWindow *parent = windows + 126;
	FvwmWindow *child = windows + 127;
	Window client = FW_W(parent);
	assert(find_managed_client(client) == parent);
	for (unsigned i = 0; i < sizeof(aliases) / sizeof(*aliases); i++)
	{
		XSaveContext(dpy, aliases[i], FvwmContext, (char *)parent);
		assert(find_managed_client(aliases[i]) == NULL);
		XDeleteContext(dpy, aliases[i], FvwmContext);
	}
	assert(find_managed_client(0x50000000UL) == NULL);
	puts("client-context-and-alias-identity: passed");

	XDeleteContext(dpy, client, FvwmContext);
	assert(find_managed_client(client) == parent);
	XSaveContext(dpy, client, FvwmContext, (char *)parent);
	puts("construction-interval-fallback: passed");

	windows[125].next = child;
	child->prev = windows + 125;
	parent->prev = parent->next = NULL;
	assert(find_managed_client(client) == NULL);
	Window first_client = FW_W(windows);
	FW_W(windows) = client;
	assert(find_managed_client(client) == windows);
	FW_W(windows) = first_client;
	puts("retirement-and-reused-xid-fallback: passed");

	assert(get_transientfor_fvwmwindow(NULL) == NULL);
	FW_W_TRANSIENTFOR(child) = FW_W(windows);
	assert(get_transientfor_fvwmwindow(child) == NULL);
	SET_TRANSIENT(child, 1);
	FW_W_TRANSIENTFOR(child) = None;
	assert(get_transientfor_fvwmwindow(child) == NULL);
	FW_W_TRANSIENTFOR(child) = Scr.Root;
	assert(get_transientfor_fvwmwindow(child) == NULL);
	FW_W_TRANSIENTFOR(child) = FW_W(child);
	assert(get_transientfor_fvwmwindow(child) == NULL);
	FW_W_TRANSIENTFOR(child) = aliases[0];
	XSaveContext(dpy, aliases[0], FvwmContext, (char *)windows);
	assert(get_transientfor_fvwmwindow(child) == NULL);
	XDeleteContext(dpy, aliases[0], FvwmContext);
	puts("transient-root-self-and-alias-guards: passed");

	context_lookups = 0;
	for (unsigned i = 1; i < 126; i++)
	{
		SET_TRANSIENT(windows + i, 1);
		FW_W_TRANSIENTFOR(windows + i) = FW_W(windows + i - 1);
		assert(get_transientfor_fvwmwindow(windows + i) ==
			windows + i - 1);
	}
	assert(context_lookups == 125);
	for (unsigned i = 0; i < 125; i++)
	{
		SET_TRANSIENT(windows + i, 1);
		FW_W_TRANSIENTFOR(windows + i) = FW_W(child);
		assert(get_transientfor_fvwmwindow(windows + i) == child);
	}
	assert(context_lookups == 250);
	puts("indexed-deep-and-wide-parent-selection: passed");
	for (unsigned i = 0; i < 128; i++)
	{
		XDeleteContext(dpy, FW_W(windows + i), FvwmContext);
	}
	Scr.FvwmRoot.next = NULL;
	fflush(stdout);
	_exit(0);
}
