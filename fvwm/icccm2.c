/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <poll.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "libs/log.h"
#include "libs/XError.h"
#include "libs/fvwmsignal.h"
#include "event_loop_policy.h"
#include "events.h"
#include "libs/atom_batch.h"
#include "libs/fvwm_x11.h"
#include "libs/FEvent.h"
#include "libs/FProperty.h"
#include "property_policy.h"
#include <stdint.h>
#include "fvwm.h"
#include "externs.h"
#include "execcontext.h"
#include "misc.h"
#include "screen.h"
#include "eventmask.h"

Time managing_since;

Atom _XA_WM_SX;
Atom _XA_MANAGER;
Atom _XA_ATOM_PAIR;
Atom _XA_WM_COLORMAP_NOTIFY;
#define _XA_TARGETS   conversion_targets[0]
#define _XA_MULTIPLE  conversion_targets[1]
#define _XA_TIMESTAMP conversion_targets[2]
#define _XA_VERSION   conversion_targets[3]
#define MAX_TARGETS                      4

long conversion_targets[MAX_TARGETS];

long icccm_version[] = { 2, 0 };

/* Only the old-owner watch/query may race its window's destruction. */
static int manager_window_error(Display *display, XErrorEvent *error)
{
	if (error->error_code == BadWindow)
	{
		return 0;
	}
	return ferror_call_next_error_handler(display, error);
}

static Bool manager_watch_owner(Window owner)
{
	XWindowAttributes attributes;
	Status exists;

	XSync(dpy, False);
	ferror_set_temp_error_handler(manager_window_error);
	XSelectInput(dpy, owner, StructureNotifyMask);
	exists = XGetWindowAttributes(dpy, owner, &attributes);
	XSync(dpy, False);
	ferror_reset_temp_error_handler();
	return exists != 0;
}

/* Startup has no command/event dispatcher yet. Consume only the old owner's
 * destruction, leaving every unrelated event for the normal startup loop. */
static const char *manager_wait_for_owner(Window owner)
{
	fvwm_module_deadline deadline;
	struct pollfd descriptor = { ConnectionNumber(dpy), POLLIN, 0 };
	XEvent event;
	int remaining;

	if (!fvwm_module_deadline_init(&deadline, events_monotonic_ns(), 10))
	{
		return "unable to initialize manager handoff deadline";
	}
	for (;;)
	{
		if (isTerminated)
		{
			return "manager handoff cancelled by termination";
		}
		if (fvwm_module_deadline_poll_remaining(
		    &deadline, events_monotonic_ns(), &remaining) !=
		    FVWM_MODULE_DEADLINE_READY)
		{
			return "manager handoff timed out or clock failed";
		}
		if (XGetSelectionOwner(dpy, _XA_WM_SX) != Scr.NoFocusWin)
		{
			return "selection ownership lost during manager handoff";
		}
		if (FCheckTypedWindowEvent(dpy, owner, DestroyNotify, &event))
		{
			return NULL;
		}
		/* A short finite slice also covers signal-before-poll races. */
		if (fvwmPoll(&descriptor, 1, remaining < 100 ? remaining : 100) < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return "manager handoff readiness wait failed";
		}
		if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			return "X connection lost during manager handoff";
		}
	}
}

void
SetupICCCM2(Bool replace_wm)
{
	Window running_wm_win;
	XSetWindowAttributes attr;
	XEvent event = { 0 };
	XClientMessageEvent *ev = &event.xclient;
	const char *failure;
	char wm_sx[25];

	snprintf(wm_sx, sizeof(wm_sx), "WM_S%lu", Scr.screen);
	/* All names are independent; finish interning before selection ownership. */
	char *names[] = { wm_sx, "MANAGER", "ATOM_PAIR", "TARGETS",
		"MULTIPLE", "TIMESTAMP", "VERSION", "WM_COLORMAP_NOTIFY" };
	Atom atoms[sizeof(names) / sizeof(*names)];
	atom_batch_intern(dpy, names, sizeof(names) / sizeof(*names), atoms);
	_XA_WM_SX = atoms[0];
	_XA_MANAGER = atoms[1];
	_XA_ATOM_PAIR = atoms[2];
	_XA_TARGETS = atoms[3];
	_XA_MULTIPLE = atoms[4];
	_XA_TIMESTAMP = atoms[5];
	_XA_VERSION = atoms[6];
	_XA_WM_COLORMAP_NOTIFY = atoms[7];

	/* Check for a running ICCCM 2.0 compliant WM */
	running_wm_win = XGetSelectionOwner(dpy, _XA_WM_SX);
	if (running_wm_win != None)
	{
		if (!replace_wm)
		{
			failure = "another ICCCM 2.0 compliant WM is running,"
				" try -replace";
			goto failed;
		}
		/* We need to know when the old manager is gone.
		   Thus we wait until it destroys running_wm_win. */
		if (!manager_watch_owner(running_wm_win))
		{
			running_wm_win = None;
		}
	}

	/* We are not yet in the event loop, thus fev_get_evtime() will not
	 * be ready.  Have to get a timestamp manually by provoking a
	 * PropertyNotify. */
	managing_since = get_server_time();

	XSetSelectionOwner(dpy, _XA_WM_SX, Scr.NoFocusWin, managing_since);
	if (XGetSelectionOwner(dpy, _XA_WM_SX) != Scr.NoFocusWin)
	{
		failure = "failed to acquire selection ownership";
		goto failed;
	}

	/* Announce the acquisition immediately, including its actual owner. */
	ev->type = ClientMessage;
	ev->display = dpy;
	ev->window = Scr.Root;
	ev->message_type = _XA_MANAGER;
	ev->format = 32;
	ev->data.l[0] = managing_since;
	ev->data.l[1] = _XA_WM_SX;
	ev->data.l[2] = Scr.NoFocusWin;
	FSendEvent(dpy, Scr.Root, False, StructureNotifyMask, &event);

	if (running_wm_win != None)
	{
		failure = manager_wait_for_owner(running_wm_win);
		if (failure != NULL)
		{
			goto failed;
		}
	}
	/* The selection can change while the old manager is releasing X state. */
	if (isTerminated ||
	    XGetSelectionOwner(dpy, _XA_WM_SX) != Scr.NoFocusWin)
	{
		failure = "manager acquisition cancelled or superseded";
		goto failed;
	}

	/* restore NoFocusWin event mask */
	attr.event_mask = XEVMASK_NOFOCUSW;
	XChangeWindowAttributes(dpy, Scr.NoFocusWin, CWEventMask, &attr);

	return;

failed:
	fvwm_debug(__func__, "%s", failure);
	/* Close only our startup connection/resources. Never clear a successor's
	 * selection or destroy the old manager's window to force a takeover. */
	XCloseDisplay(dpy);
	dpy = NULL;
	exit(1);
}

/* We must make sure that we have released SubstructureRedirect
   before we destroy manager_win, so that another wm can start
   successfully. */
void
CloseICCCM2(void)
{
	XSelectInput(dpy, Scr.Root, NoEventMask);
	XFlush(dpy);

	return;
}

/* Conversion output and SelectionNotify use the same ordered connection.
 * Asynchronous server allocation errors retain the normal X error policy. */
static Bool
convertProperty(Window w, Atom target, Atom property)
{
	if (target == _XA_TARGETS)
	{
		XChangeProperty(
			dpy, w, property, XA_ATOM, 32, PropModeReplace,
			(unsigned char *)conversion_targets, MAX_TARGETS);
	}
	else if (target == _XA_TIMESTAMP)
	{
		long local_managing_since;

		local_managing_since = managing_since;
		XChangeProperty(
			dpy, w, property, XA_INTEGER, 32, PropModeReplace,
			(unsigned char *)&local_managing_since, 1);
	}
	else if (target == _XA_VERSION)
	{
		XChangeProperty(
			dpy, w, property, XA_INTEGER, 32, PropModeReplace,
			(unsigned char *)icccm_version, 2);
	}
	else
	{
		return False;
	}
	/* Flushing submits writes; it is not a server-allocation success check. */
	XFlush(dpy);

	return True;
}

void
icccm2_handle_selection_request(const XEvent *e)
{
	XSelectionRequestEvent ev = e->xselectionrequest;
	XEvent response = { 0 };
	XSelectionEvent *reply = &response.xselection;

	reply->type = SelectionNotify;
	reply->display = dpy;
	reply->requestor = ev.requestor;
	reply->selection = ev.selection;
	reply->target = ev.target;
	reply->property = None;
	reply->time = ev.time;

	/* Dispatch also receives synthetic events. Only this manager role and
	 * acquisition period may publish our conversion data. No X round trip
	 * is needed: ownership loss is handled by the ordered SelectionClear. */
	if (ev.selection != _XA_WM_SX || ev.owner != Scr.NoFocusWin ||
	    !fvwm_selection_request_time_is_valid(ev.time, managing_since))
	{
		fvwm_debug(__func__, "Refusing foreign or stale manager request");
		goto notify;
	}

	if (ev.target == _XA_MULTIPLE)
	{
		if (ev.property != None)
		{
			unsigned char *data;
			size_t bytes;
			FPropertyLimits limits = {.min_bytes = 0,
			    .max_bytes = FVWM_SELECTION_PAIR_MAX_BYTES, .format = 32};
			FPropertyResult result = FPropertyRead(dpy, ev.requestor,
			    ev.property, _XA_ATOM_PAIR, limits, &data, &bytes);

			if (result == FPROPERTY_OK &&
			    fvwm_selection_pair_length_is_valid(bytes))
			{
				const uint32_t *values = (const uint32_t *)data;
				unsigned long pairs[FVWM_SELECTION_PAIR_MAX_BYTES / 4];
				size_t count = bytes / 4;

				/* XChangeProperty expects native longs for format 32. */
				for (size_t i = 0; i < count; i++)
				{
					pairs[i] = values[i];
				}
				for (size_t i = 0; i < count; i += 2)
				{
					if (pairs[i + 1] == None || !convertProperty(
					    ev.requestor, pairs[i], pairs[i + 1]))
					{
						pairs[i + 1] = None;
					}
				}
				XChangeProperty(dpy, ev.requestor, ev.property,
				    _XA_ATOM_PAIR, 32, PropModeReplace,
				    (unsigned char *)pairs, (int)count);
				reply->property = ev.property;
			}
			else
			{
				fvwm_debug(__func__, "Ignoring invalid MULTIPLE pairs");
			}
			free(data);
		}
	}
	else
	{
		if (ev.property == None)
		{
			ev.property = ev.target;
		}
		if (convertProperty(ev.requestor, ev.target, ev.property))
		{
			reply->property = ev.property;
		}
	}
notify:
	FSendEvent(dpy, ev.requestor, False, 0L, &response);
	XFlush(dpy);

	return;
}

/* If another wm is requesting ownership of the selection,
   we receive a SelectionClear event. In that case, we have to
   release all resources and destroy manager_win. Done() calls
   CloseICCCM2() after undecorating all windows. */
void
icccm2_handle_selection_clear(void)
{
	Done(0, NULL);

	return;
}
