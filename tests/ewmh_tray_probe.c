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
#include "fvwm/fvwm.h"
#include "fvwm/ewmh.h"
#include "fvwm/ewmh_intern.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "ewmh_tray_probe.h"

static Bool active, refuse;
static Window expected[70], subject;
static Atom tray_atom;
static unsigned count, publications, selections, flushes, stage, appends;
static unsigned snapshot_allocations, cases, refusals;
static int operation;

static Bool baseline(void)
{
	return getenv("FVWM_TRAY_BASELINE") != NULL;
}

void ewmh_tray_allocation(size_t bytes)
{
	if (active && bytes == 64 * sizeof(Window))
		snapshot_allocations++;
}

bool ewmh_tray_append_refused(ewmh_client_list *list, Window window)
{
	if (!active)
		return false;
	assert(list->count < count && expected[list->count] == window);
	appends++;
	if (refuse && list->count == 64)
	{
		refuse = False;
		refusals++;
		return true;
	}
	return false;
}

void ewmh_tray_publication(Window window, Atom property, Atom type,
	int format, int mode, const unsigned char *data, int length)
{
	if (!active || property != tray_atom)
		return;
	assert(window == Scr.Root && type == XA_WINDOW && format == 32);
	assert(mode == PropModeReplace && length == (int)count);
	assert(operation == DestroyNotify ? stage == 2 : stage == 0);
	for (unsigned i = 0; i < count; i++)
		assert(((const Window *)data)[i] == expected[i]);
	assert(EWMH_IsKdeSysTrayWindow(subject) ==
		(operation == DestroyNotify ? 0 : 1));
	stage = 3;
	publications++;
}

int __real_XSelectInput(Display *, Window, long);
int __wrap_XSelectInput(Display *display, Window window, long mask)
{
	if (active)
	{
		assert(window == subject && EWMH_IsKdeSysTrayWindow(window));
		assert(stage == 0);
		assert(mask == (operation == DestroyNotify ?
			NoEventMask : StructureNotifyMask));
		stage = 1;
		selections++;
	}
	return __real_XSelectInput(display, window, mask);
}

int __real_XFlush(Display *);
int __wrap_XFlush(Display *display)
{
	if (active)
	{
		assert(stage == 1);
		stage = 2;
		flushes++;
	}
	return __real_XFlush(display);
}

static void check_property(const Window *values, unsigned length)
{
	Atom type;
	int format;
	unsigned long items, after;
	unsigned char *data = NULL;

	assert(XGetWindowProperty(dpy, Scr.Root, tray_atom, 0, 128, False,
		XA_WINDOW, &type, &format, &items, &after, &data) == Success);
	assert(type == XA_WINDOW && format == 32 && items == length && !after);
	for (unsigned i = 0; i < length; i++)
		assert(((Window *)data)[i] == values[i]);
	XFree(data);
}

static void start(Window window, int type)
{
	subject = window;
	operation = type;
	publications = selections = flushes = stage = appends = 0;
	snapshot_allocations = 0;
	active = True;
}

static void finish(unsigned published, unsigned selected)
{
	active = False;
	assert(publications == published && selections == selected);
	assert(flushes == selected);
	cases++;
}

static void add(Window window, Bool inserted, Bool publish)
{
	FvwmWindow fw = { 0 };

	if (inserted)
	{
		assert(count < 70);
		expected[count++] = window;
	}
	FW_W(&fw) = window;
	start(window, MapNotify);
	ewmh_AddToKdeSysTray(&fw);
	finish(publish, 0);
	{
		Bool present = False;

		for (unsigned i = 0; i < count; i++)
			present |= expected[i] == window;
		assert(EWMH_IsKdeSysTrayWindow(window) == present);
	}
	if (publish)
		check_property(expected, count);
}

static void remove_window(unsigned slot, Bool publish)
{
	Window window = expected[slot];

	assert(slot < count);
	memmove(expected + slot, expected + slot + 1,
		(--count - slot) * sizeof(*expected));
	start(window, DestroyNotify);
	EWMH_ManageKdeSysTray(window, DestroyNotify);
	finish(publish, 1);
	assert(!EWMH_IsKdeSysTrayWindow(window));
	if (publish)
		check_property(expected, count);
}

static void event(Window window, int type, Bool selected)
{
	XWindowAttributes attributes;

	start(window, type);
	EWMH_ManageKdeSysTray(window, type);
	finish(0, selected);
	if (selected)
	{
		assert(XGetWindowAttributes(dpy, window, &attributes));
		assert(attributes.your_event_mask == StructureNotifyMask);
	}
	check_property(expected, count);
}

void ewmh_tray_probe(void)
{
	Window windows[68], value = Scr.Root;
	Atom marker = XInternAtom(dpy, "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR",
		False);
	unsigned add_snapshot_allocations, remove_snapshot_allocations;

	tray_atom = XInternAtom(dpy, "_KDE_NET_SYSTEM_TRAY_WINDOWS", False);
	for (unsigned i = 0; i < 68; i++)
	{
		windows[i] = XCreateSimpleWindow(dpy, Scr.Root, 0, 0, 1, 1,
			0, 0, 0);
		if (i < 66)
			XChangeProperty(dpy, windows[i], marker, XA_WINDOW, 32,
				PropModeReplace, (unsigned char *)&value, 1);
	}
	/* Absent/mistyped markers do not acquire membership or publish. */
	add(windows[66], False, False);
	XChangeProperty(dpy, windows[67], marker, XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)&value, 1);
	add(windows[67], False, False);
	assert(!EWMH_IsKdeSysTrayWindow(windows[66]));
	assert(!EWMH_IsKdeSysTrayWindow(windows[67]));
	for (unsigned i = 0; i < 64; i++)
		add(windows[i], True, True);
	add_snapshot_allocations = snapshot_allocations;
	assert(add_snapshot_allocations == (baseline() ? 1 : 0));
	assert(appends == (baseline() ? 0 : 64));
	/* The duplicate marker read remains, without another publication. */
	add(windows[27], False, False);
	assert(EWMH_IsKdeSysTrayWindow(windows[27]));
	event(windows[66], UnmapNotify, False);
	event(windows[27], UnmapNotify, True);
	event(windows[27], ReparentNotify, True);
	event(windows[27], ClientMessage, False);
	add(windows[64], True, True);
	assert(appends == (baseline() ? 0 : 65));
	remove_window(32, True);
	remove_snapshot_allocations = snapshot_allocations;
	assert(remove_snapshot_allocations == (baseline() ? 1 : 0));
	assert(appends == (baseline() ? 0 : 64));
	if (!baseline())
	{
		Window saved[70];
		unsigned saved_count = count;

		memcpy(saved, expected, count * sizeof(*expected));
		/* Refusal keeps the old property, while membership commits. */
		refuse = True;
		add(windows[65], True, False);
		assert(!refuse);
		check_property(saved, saved_count);
		add(windows[32], True, True);
		memcpy(saved, expected, count * sizeof(*expected));
		saved_count = count;
		refuse = True;
		remove_window(count - 1, False);
		assert(!refuse);
		check_property(saved, saved_count);
		remove_window(count - 1, True);
	}
	/* Head/middle/tail retirement preserves insertion order. */
	remove_window(0, True);
	remove_window(count / 2, True);
	remove_window(count - 1, True);
	while (count)
		remove_window(0, True);
	assert(!EWMH_IsKdeSysTrayWindow(windows[27]));
	event(windows[27], DestroyNotify, False);
	/* Fixed marker records reject empty values, but accept a zero ID. */
	XChangeProperty(dpy, windows[66], marker, XA_WINDOW, 32,
		PropModeReplace, NULL, 0);
	add(windows[66], False, False);
	value = None;
	XChangeProperty(dpy, windows[66], marker, XA_WINDOW, 32,
		PropModeReplace, (unsigned char *)&value, 1);
	add(windows[66], True, True);
	remove_window(0, True);
	for (unsigned i = 0; i < 68; i++)
		XDestroyWindow(dpy, windows[i]);
	XSync(dpy, False);
	printf("STATE_TRAY_WORK {\"baseline\":%u,\"cases\":%u,"
		"\"add_64_snapshot_allocations\":%u,"
		"\"remove_64_snapshot_allocations\":%u,\"refusals\":%u}\n",
		baseline(), cases, add_snapshot_allocations,
		remove_snapshot_allocations, refusals);
	puts("legacy-tray membership/order/events/inline/refusal/repair: passed");
}
