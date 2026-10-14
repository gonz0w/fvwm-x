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
#include "fvwm/ewmh_intern.h"

extern ewmh_atom_list atom_list[];
extern Atom XA_UTF8_STRING, _XA_WM_SX, _XA_MANAGER, _XA_ATOM_PAIR;
extern Atom _XA_WM_COLORMAP_NOTIFY, _XA_MwmAtom, _XA_MOTIF_WM;
extern long conversion_targets[];
extern Status __real_XInternAtoms(Display *, char **, int, Bool, Atom *);
extern Atom __real_XInternAtom(Display *, const char *, Bool);

static int batch_calls, owned_single_calls;
static int batch_sizes[3];
static const struct { const char *name; Atom *value; } core_atoms[] = {
	{ "_MIT_PRIORITY_COLORS", &_XA_MIT_PRIORITY_COLORS },
	{ "WM_CHANGE_STATE", &_XA_WM_CHANGE_STATE },
	{ "WM_STATE", &_XA_WM_STATE },
	{ "WM_COLORMAP_WINDOWS", &_XA_WM_COLORMAP_WINDOWS },
	{ "WM_PROTOCOLS", &_XA_WM_PROTOCOLS },
	{ "WM_TAKE_FOCUS", &_XA_WM_TAKE_FOCUS },
	{ "WM_DELETE_WINDOW", &_XA_WM_DELETE_WINDOW },
	{ "WM_DESKTOP", &_XA_WM_DESKTOP },
	{ "_MOTIF_WM_HINTS", &_XA_MwmAtom },
	{ "_MOTIF_WM_INFO", &_XA_MOTIF_WM },
	{ "_OL_WIN_ATTR", &_XA_OL_WIN_ATTR },
	{ "_OL_WT_BASE", &_XA_OL_WT_BASE },
	{ "_OL_WT_CMD", &_XA_OL_WT_CMD },
	{ "_OL_WT_HELP", &_XA_OL_WT_HELP },
	{ "_OL_WT_NOTICE", &_XA_OL_WT_NOTICE },
	{ "_OL_WT_OTHER", &_XA_OL_WT_OTHER },
	{ "_OL_DECOR_ADD", &_XA_OL_DECOR_ADD },
	{ "_OL_DECOR_DEL", &_XA_OL_DECOR_DEL },
	{ "_OL_DECOR_CLOSE", &_XA_OL_DECOR_CLOSE },
	{ "_OL_DECOR_RESIZE", &_XA_OL_DECOR_RESIZE },
	{ "_OL_DECOR_HEADER", &_XA_OL_DECOR_HEADER },
	{ "_OL_DECOR_ICON_NAME", &_XA_OL_DECOR_ICON_NAME },
	{ "WM_WINDOW_ROLE", &_XA_WM_WINDOW_ROLE },
	{ "WINDOW_ROLE", &_XA_WINDOW_ROLE },
	{ "WM_CLIENT_LEADER", &_XA_WM_CLIENT_LEADER },
	{ "SM_CLIENT_ID", &_XA_SM_CLIENT_ID },
	{ "_XROOTPMAP_ID", &_XA_XROOTPMAP_ID },
	{ "_XSETROOT_ID", &_XA_XSETROOT_ID },
};

static Bool owned_name(const char *name)
{
	for (size_t i = 0; i < sizeof(core_atoms) / sizeof(*core_atoms); i++)
	{
		if (strcmp(name, core_atoms[i].name) == 0)
		{
			return True;
		}
	}
	if (strcmp(name, "UTF8_STRING") == 0 ||
	    strcmp(name, "MANAGER") == 0 || strcmp(name, "WM_S0") == 0 ||
	    strcmp(name, "ATOM_PAIR") == 0 || strcmp(name, "TARGETS") == 0 ||
	    strcmp(name, "MULTIPLE") == 0 || strcmp(name, "TIMESTAMP") == 0 ||
	    strcmp(name, "VERSION") == 0 ||
	    strcmp(name, "WM_COLORMAP_NOTIFY") == 0)
	{
		return True;
	}
	for (ewmh_atom_list *list = atom_list; list->list != NULL; list++)
	{
		for (ewmh_atom *entry = list->list; entry->name != NULL; entry++)
		{
			if (strcmp(name, entry->name) == 0)
			{
				return True;
			}
		}
	}
	return False;
}

Atom __wrap_XInternAtom(Display *display, const char *name, Bool only_if_exists)
{
	if (owned_name(name))
	{
		owned_single_calls++;
	}
	return __real_XInternAtom(display, name, only_if_exists);
}

Status __wrap_XInternAtoms(Display *display, char **names, int count,
	Bool only_if_exists, Atom *atoms)
{
	assert(batch_calls < 3 && count > 0 && !only_if_exists);
	batch_sizes[batch_calls++] = count;
	Status status = __real_XInternAtoms(display, names, count, only_if_exists,
		atoms);
	assert(status);
	if (getenv("FVWM_ATOM_PARTIAL") != NULL)
	{
		atoms[0] = None;
		return 0;
	}
	return status;
}

static void assert_atom_name(Atom atom, const char *expected)
{
	assert(atom != None);
	char *name = XGetAtomName(dpy, atom);
	assert(name != NULL && strcmp(name, expected) == 0);
	XFree(name);
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	Bool partial = getenv("FVWM_ATOM_PARTIAL") != NULL;
	/* Colorset initialization retains its independent _XROOTPMAP_ID call. */
	assert(batch_calls == 3 && owned_single_calls == (partial ? 4 : 1));
	assert(batch_sizes[0] == 28 && batch_sizes[1] == 8);
	for (size_t i = 0; i < sizeof(core_atoms) / sizeof(*core_atoms); i++)
	{
		assert_atom_name(*core_atoms[i].value, core_atoms[i].name);
	}
	assert_atom_name(_XA_WM_SX, "WM_S0");
	assert_atom_name(_XA_MANAGER, "MANAGER");
	assert_atom_name(_XA_ATOM_PAIR, "ATOM_PAIR");
	assert_atom_name(_XA_WM_COLORMAP_NOTIFY, "WM_COLORMAP_NOTIFY");
	const char *selection_names[] = { "TARGETS", "MULTIPLE", "TIMESTAMP",
		"VERSION" };
	for (int i = 0; i < 4; i++)
	{
		assert_atom_name(conversion_targets[i], selection_names[i]);
	}
	assert(XGetSelectionOwner(dpy, _XA_WM_SX) == Scr.NoFocusWin);
	assert_atom_name(XA_UTF8_STRING, "UTF8_STRING");
	Atom property = __real_XInternAtom(dpy, "_NET_SUPPORTED", True);
	Atom actual_type;
	int format;
	unsigned long count, remaining;
	unsigned char *bytes = NULL;
	assert(XGetWindowProperty(dpy, Scr.Root, property, 0, 256, False,
		XA_ATOM, &actual_type, &format, &count, &remaining, &bytes)
		== Success);
	assert(actual_type == XA_ATOM && format == 32 && remaining == 0);
	unsigned long index = 0;
	for (ewmh_atom_list *list = atom_list; list->list != NULL; list++)
	{
		for (ewmh_atom *entry = list->list; entry->name != NULL; entry++)
		{
			assert_atom_name(entry->atom, entry->name);
			assert(index < count && ((Atom *)bytes)[index++] == entry->atom);
		}
	}
	assert(index == count && batch_sizes[2] == count + 1);
	XFree(bytes);
	printf("atom-mapping-selection-supported: passed\n");
	printf("atom-batches: %d/%d/%d; partial-fallback: %d\n",
		batch_sizes[0], batch_sizes[1], batch_sizes[2], owned_single_calls - 1);
	fflush(stdout);
	_exit(0);
}
