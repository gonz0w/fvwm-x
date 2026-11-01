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
#include "icccm2.h"

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
#include "manager_selection.h"
#include <stdint.h>
#include "fvwm.h"
#include "externs.h"
#include "execcontext.h"
#include "misc.h"
#include "screen.h"
#include "eventmask.h"

Time managing_since;
static uint64_t manager_acquired_ns;
static manager_selection manager_transport;

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
		"MULTIPLE", "TIMESTAMP", "VERSION", "WM_COLORMAP_NOTIFY",
		"_FVWM_MANAGER_CONVERSION_ACK" };
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
	manager_acquired_ns = events_monotonic_ns();
	managing_since = get_server_time();

	XSetSelectionOwner(dpy, _XA_WM_SX, Scr.NoFocusWin, managing_since);
	if (XGetSelectionOwner(dpy, _XA_WM_SX) != Scr.NoFocusWin)
	{
		failure = "failed to acquire selection ownership";
		goto failed;
	}

	Atom manager_targets[MAX_TARGETS];

	for (size_t i = 0; i < MAX_TARGETS; i++)
		manager_targets[i] = (Atom)conversion_targets[i];
	if (!manager_selection_init(&manager_transport, dpy, Scr.Root,
	    Scr.NoFocusWin, _XA_WM_SX, _XA_ATOM_PAIR, atoms[8], manager_targets,
	    managing_since, manager_acquired_ns))
	{
		failure = "unable to initialize manager conversion state";
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
	manager_selection_close(&manager_transport);
	XSelectInput(dpy, Scr.Root, NoEventMask);
	XFlush(dpy);

	return;
}

/* Metadata writes complete on the ordinary X connection without a new
 * synchronous barrier. Queue/deadline/error ownership stays manager-local. */
void
icccm2_handle_selection_request(const XEvent *event)
{
	manager_selection_admit(&manager_transport, &event->xselectionrequest,
		events_monotonic_ns());
	icccm2_service_selection();
}

Bool icccm2_handle_selection_event(const XEvent *event)
{
	return manager_selection_event(&manager_transport, event);
}

Bool icccm2_defer_selection_event(const XEvent *event)
{
	return manager_selection_defer_event(&manager_transport, event);
}

void icccm2_selection_interaction(void)
{
	manager_selection_interaction(&manager_transport);
	manager_selection_service(&manager_transport);
}

Bool icccm2_handle_selection_error(const XErrorEvent *event)
{
	return manager_selection_error(&manager_transport, event);
}

void icccm2_service_selection(void)
{
	manager_selection_ordinary(&manager_transport);
}

int icccm2_selection_timeout_ms(int other_ms)
{
	int manager_ms = manager_selection_timeout_ms(&manager_transport,
		events_monotonic_ns());

	return manager_ms >= 0 && (other_ms < 0 || manager_ms < other_ms) ?
		manager_ms : other_ms;
}

/* If another wm is requesting ownership of the selection,
   we receive a SelectionClear event. In that case, we have to
   release all resources and destroy manager_win. Done() calls
   CloseICCCM2() after undecorating all windows. */
void
icccm2_handle_selection_clear(const XSelectionClearEvent *event)
{
	if (!manager_selection_clear(&manager_transport, event))
		return;
	Done(0, NULL);

	return;
}
