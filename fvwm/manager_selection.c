/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "manager_selection.h"
#include "events.h"
#include "libs/FEvent.h"
#include "libs/FProperty.h"
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool manager_is_parent(const manager_selection *state)
{
	return state->display != NULL && state->process == getpid();
}

static int manager_remaining_ms(uint64_t admitted, uint64_t now)
{
	if (now == UINT64_MAX || admitted == UINT64_MAX || now < admitted ||
	    now - admitted >= MANAGER_SELECTION_LIFETIME_NS)
	{
		return 0;
	}
	return (int)((MANAGER_SELECTION_LIFETIME_NS - (now - admitted) +
		UINT64_C(999999)) / UINT64_C(1000000));
}

static void manager_notify(manager_selection *state,
	const XSelectionRequestEvent *request, Atom property)
{
	XEvent response = {0};
	XSelectionEvent *reply = &response.xselection;

	reply->type = SelectionNotify;
	reply->display = state->display;
	reply->requestor = request->requestor;
	reply->selection = request->selection;
	reply->target = request->target;
	reply->property = property;
	reply->time = request->time;
	FSendEvent(state->display, request->requestor, False, 0, &response);
	XFlush(state->display);
}

bool manager_selection_init(manager_selection *state, Display *display,
	Window root, Window owner, Atom selection, Atom atom_pair,
	Atom marker_atom, const Atom targets[4], Time acquired,
	uint64_t acquired_ns)
{
	XSetWindowAttributes attributes = {0};

	if (state->display != NULL || display == NULL || root == None ||
	    owner == None || selection == None || atom_pair == None ||
	    marker_atom == None || targets == NULL)
	{
		return false;
	}
	for (size_t i = 0; i < 4; i++)
	{
		if (targets[i] == None)
			return false;
	}
	state->display = display;
	state->process = getpid();
	state->owner = owner;
	state->selection = selection;
	state->atom_pair = atom_pair;
	state->marker_atom = marker_atom;
	memcpy(state->targets, targets, sizeof(state->targets));
	state->acquired = acquired;
	state->acquired_ns = acquired_ns;
	attributes.event_mask = PropertyChangeMask;
	state->creation_serial = NextRequest(display);
	state->creation_pending = true;
	state->marker_window = XCreateWindow(display, root, 0, 0, 1, 1, 0,
		0, InputOnly, CopyFromParent, CWEventMask, &attributes);
	state->active = state->marker_window != None;
	return state->active;
}

static bool manager_request_is_current(const manager_selection *state,
	const XSelectionRequestEvent *event, uint64_t now_ns)
{
	return state->active && !state->closing && !state->loss_pending &&
	    event->display == state->display &&
	    event->selection == state->selection && event->owner == state->owner &&
	    fvwm_selection_request_time_is_valid(event->time, state->acquired,
		state->acquired_ns, now_ns);
}

void manager_selection_admit(manager_selection *state,
	const XSelectionRequestEvent *event, uint64_t now_ns)
{
	if (!manager_is_parent(state) || state->closing)
		return;
	if (!manager_request_is_current(state, event, now_ns) ||
	    state->marker_unusable || now_ns == UINT64_MAX ||
	    state->count == MANAGER_SELECTION_QUEUE_MAX)
	{
		manager_notify(state, event, None);
		return;
	}
	size_t tail = (state->head + state->count) % MANAGER_SELECTION_QUEUE_MAX;

	state->queue[tail].event = *event;
	state->queue[tail].admitted_ns = now_ns;
	state->count++;
}

static void manager_record_write(manager_selection *state, size_t pair,
	Atom property, Atom type, const unsigned long *values, int count)
{
	manager_selection_conversion *current = &state->current;
	manager_selection_write *write = &current->writes[current->write_count++];

	/* Register before Xlib can flush and deliver an error callback. */
	write->serial = NextRequest(state->display);
	write->pair = pair;
	write->failed = false;
	XChangeProperty(state->display, current->request.event.requestor,
		property, type, 32, PropModeReplace,
		(const unsigned char *)values, count);
}

static bool manager_convert(manager_selection *state, size_t pair,
	Atom target, Atom property)
{
	if (target == state->targets[0])
	{
		manager_record_write(state, pair, property, XA_ATOM,
			state->targets, 4);
	}
	else if (target == state->targets[2])
	{
		unsigned long timestamp = state->acquired;

		manager_record_write(state, pair, property, XA_INTEGER,
			&timestamp, 1);
	}
	else if (target == state->targets[3])
	{
		const unsigned long version[] = {2, 0};

		manager_record_write(state, pair, property, XA_INTEGER, version, 2);
	}
	else
	{
		return false;
	}
	return true;
}

static void manager_marker(manager_selection *state)
{
	manager_selection_conversion *current = &state->current;
	unsigned long marker = NextRequest(state->display);

	current->marker_serial = marker;
	current->marker_submitted = true;
	current->marker_settled = false;
	current->marker_failed = false;
	XChangeProperty(state->display, state->marker_window, state->marker_atom,
		XA_CARDINAL, 32, PropModeReplace, (const unsigned char *)&marker, 1);
	XFlush(state->display);
}

static bool manager_begin(manager_selection *state,
	const manager_selection_request *request)
{
	manager_selection_conversion *current = &state->current;
	const XSelectionRequestEvent *event = &request->event;

	memset(current, 0, sizeof(*current));
	current->inflight = true;
	current->request = *request;
	state->deferred_marker_valid = false;
	current->multiple = event->target == state->targets[1];
	if (manager_remaining_ms(request->admitted_ns,
	    events_monotonic_ns()) == 0 || state->marker_unusable ||
	    state->loss_pending || !state->active)
	{
		current->inflight = false;
		return false;
	}
	if (current->multiple)
	{
		unsigned char *data = NULL;
		size_t bytes = 0;
		FPropertyLimits limits = {0, FVWM_SELECTION_PAIR_MAX_BYTES, 32};

		/* Retain the existing bounded control snapshot read. No new
		 * synchronous query is added to write/error completion. */
		FPropertyResult result = event->property == None ? FPROPERTY_MISSING :
			FPropertyRead(state->display, event->requestor,
				event->property, state->atom_pair, limits, &data, &bytes);

		if (result != FPROPERTY_OK ||
		    !fvwm_selection_pair_length_is_valid(bytes) ||
		    manager_remaining_ms(request->admitted_ns,
			events_monotonic_ns()) == 0 || state->marker_unusable ||
		    state->loss_pending || !state->active)
		{
			free(data);
			current->inflight = false;
			return false;
		}
		const uint32_t *packed = (const uint32_t *)data;

		current->pair_count = bytes / 8;
		for (size_t i = 0; i < current->pair_count * 2; i++)
			current->pairs[i] = packed[i];
		free(data);
		for (size_t i = 0; i < current->pair_count; i++)
		{
			Atom target = current->pairs[i * 2];
			Atom destination = current->pairs[i * 2 + 1];

			if (destination == None || destination == event->property ||
			    !manager_convert(state, i, target, destination))
			{
				current->pairs[i * 2 + 1] = None;
			}
		}
	}
	else
	{
		Atom destination = event->property == None ?
			event->target : event->property;

		if (!manager_convert(state, 0, event->target, destination))
		{
			current->inflight = false;
			return false;
		}
	}
	manager_marker(state);
	return true;
}

bool manager_selection_error(manager_selection *state,
	const XErrorEvent *event)
{
	if (!manager_is_parent(state) || event->display != state->display)
		return false;
	if (state->creation_pending && event->serial == state->creation_serial &&
	    event->request_code == X_CreateWindow)
	{
		state->marker_unusable = true;
		state->creation_pending = false;
		/* Creation failure is not the terminal barrier for later writes. */
		return true;
	}
	manager_selection_conversion *current = &state->current;

	if (!current->inflight || event->request_code != X_ChangeProperty)
		return false;
	if (current->marker_submitted &&
	    event->serial == current->marker_serial)
	{
		current->marker_failed = true;
		current->marker_settled = true;
		state->marker_unusable = true;
		return true;
	}
	for (size_t i = 0; i < current->write_count; i++)
	{
		if (event->serial == current->writes[i].serial)
		{
			current->writes[i].failed = true;
			return true;
		}
	}
	return false;
}

bool manager_selection_event(manager_selection *state, const XEvent *event)
{
	if (!manager_is_parent(state) || event->type != PropertyNotify ||
	    event->xproperty.display != state->display ||
	    event->xproperty.window != state->marker_window ||
	    event->xproperty.atom != state->marker_atom)
	{
		return false;
	}
	manager_selection_conversion *current = &state->current;

	if (!event->xproperty.send_event &&
	    event->xproperty.state == PropertyNewValue && current->inflight &&
	    current->marker_submitted &&
	    event->xproperty.serial == current->marker_serial)
	{
		/* Earlier errors on this connection have reached their handler.
		 * Consume here before ordinary property-event coalescing. */
		current->marker_settled = true;
		state->deferred_marker_valid = false;
		state->creation_pending = false;
	}
	return true;
}

void manager_selection_service(manager_selection *state)
{
	uint64_t now_ns = events_monotonic_ns();

	if (!manager_is_parent(state) || !state->active || state->closing ||
	    state->loss_pending)
		return;
	manager_selection_conversion *current = &state->current;

	if (current->inflight)
	{
		if (!current->notified && manager_remaining_ms(
		    current->request.admitted_ns, now_ns) == 0)
		{
			current->notified = current->cancelled = true;
			manager_notify(state, &current->request.event, None);
		}
		if (current->marker_settled)
		{
			if (!current->cancelled && !current->marker_failed &&
			    current->multiple && !current->control_phase)
			{
				for (size_t i = 0; i < current->write_count; i++)
				{
					manager_selection_write *write = &current->writes[i];

					if (write->failed)
						current->pairs[write->pair * 2 + 1] = None;
				}
				current->control_phase = true;
				current->write_count = 0;
				manager_record_write(state, 0,
					current->request.event.property, state->atom_pair,
					current->pairs, (int)(current->pair_count * 2));
				manager_marker(state);
			}
			else
			{
				if (!current->notified)
				{
					Atom destination = current->request.event.property;
					bool failed = current->marker_failed ||
						current->writes[0].failed;

					if (destination == None)
						destination = current->request.event.target;
					current->notified = true;
					manager_notify(state, &current->request.event,
						failed ? None : destination);
				}
				current->inflight = false;
			}
		}
	}
	/* Expire headers even while a cancelled conversion retains its serials.
	 * At most the fixed FIFO is visited; an active wait is never overwritten. */
	for (size_t work = 0; work < MANAGER_SELECTION_QUEUE_MAX && state->count;
	    work++)
	{
		manager_selection_request *next = &state->queue[state->head];
		bool refused = state->marker_unusable ||
			manager_remaining_ms(next->admitted_ns, now_ns) == 0 ||
			!manager_request_is_current(state, &next->event, now_ns);

		if (!refused && current->inflight)
			break;
		manager_selection_request request = *next;

		state->head = (state->head + 1) % MANAGER_SELECTION_QUEUE_MAX;
		state->count--;
		if (refused || !manager_begin(state, &request))
			manager_notify(state, &request.event, None);
		else
			break;
	}
}

int manager_selection_timeout_ms(const manager_selection *state,
	uint64_t now_ns)
{
	int remaining = -1;
	const manager_selection_conversion *current = &state->current;

	if (!manager_is_parent(state) || !state->active || state->closing ||
	    state->loss_pending)
		return -1;
	if (current->inflight)
	{
		if (current->marker_settled)
			return 0;
		if (!current->notified)
			remaining = manager_remaining_ms(current->request.admitted_ns,
				now_ns);
	}
	if (state->count)
	{
		int queued = manager_remaining_ms(state->queue[state->head].admitted_ns,
			now_ns);

		if (!current->inflight || state->marker_unusable)
			return 0;
		if (remaining < 0 || queued < remaining)
			remaining = queued;
	}
	return remaining;
}

static bool manager_clear_is_current(const manager_selection *state,
	const XSelectionClearEvent *event)
{
	return manager_is_parent(state) && state->active && !state->closing &&
	    event->type == SelectionClear && event->display == state->display &&
	    !event->send_event && event->selection == state->selection &&
	    event->window == state->owner;
}

bool manager_selection_defer_clear(manager_selection *state,
	const XSelectionClearEvent *event)
{
	if (!manager_clear_is_current(state, event))
		return false;
	/* Close metadata admission/publication, but leave the event and owned
	 * window lifecycle to ordinary dispatch after the interaction unwinds. */
	state->loss_pending = true;
	state->current.cancelled = true;
	return true;
}

bool manager_selection_clear(manager_selection *state,
	const XSelectionClearEvent *event)
{
	if (!manager_clear_is_current(state, event))
		return false;
	/* The server's ordered loss needs neither a timestamp test nor a query.
	 * Retain in-flight serials until the caller closes its X connection. */
	state->active = false;
	state->current.cancelled = true;
	return true;
}

bool manager_selection_defer_event(manager_selection *state,
	const XEvent *event)
{
	if (!manager_is_parent(state) || event->type != PropertyNotify ||
	    event->xproperty.display != state->display ||
	    event->xproperty.window != state->marker_window ||
	    event->xproperty.atom != state->marker_atom)
	{
		return false;
	}
	manager_selection_conversion *current = &state->current;

	if (!event->xproperty.send_event &&
	    event->xproperty.state == PropertyNewValue && current->inflight &&
	    current->marker_submitted &&
	    event->xproperty.serial == current->marker_serial)
	{
		/* A masked resize extraction can skip an earlier SelectionClear.
		 * The bounded modal scan must establish order before completion. */
		state->deferred_marker = *event;
		state->deferred_marker_valid = true;
	}
	return true;
}

void manager_selection_close(manager_selection *state)
{
	if (!manager_is_parent(state) || state->closing)
		return;
	state->active = false;
	state->closing = true;
	state->current.cancelled = state->current.notified = true;
	state->count = 0;
	if (state->marker_window != None)
		XDestroyWindow(state->display, state->marker_window);
	/* Do not clear submitted serial records: XCloseDisplay can drain errors. */
}
