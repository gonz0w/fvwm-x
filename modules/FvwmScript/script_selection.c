/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "script_selection.h"
#include "libs/FEvent.h"
#include "libs/FProperty.h"
#include "libs/XError.h"
#include "libs/safemalloc.h"
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* XGetAtomName is synchronous, so its scoped BadAtom has been dispatched
 * before restoring the handler. Other pending errors retain caller policy. */
static int selection_atom_error(Display *display, XErrorEvent *error)
{
	if (error->error_code == BadAtom &&
	    error->request_code == X_GetAtomName)
		return 0;
	return ferror_call_next_error_handler(display, error);
}

static bool selection_property_valid(Display *display, Atom property)
{
	char *name;

	if (property == None)
		return false;
	ferror_set_temp_error_handler(selection_atom_error);
	name = XGetAtomName(display, property);
	ferror_reset_temp_error_handler();
	if (name == NULL)
		return false;
	XFree(name);
	return true;
}

static uint64_t selection_now_ns(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
	    (uint64_t)now.tv_sec >= UINT64_MAX / UINT64_C(1000000000))
		return UINT64_MAX;
	return (uint64_t)now.tv_sec * UINT64_C(1000000000) + now.tv_nsec;
}

static void selection_retire(script_selection_state *state,
	script_selection_owner *owner, bool release_x)
{
	if (release_x && owner->window != None)
		XDestroyWindow(state->display, owner->window);
	free(owner->text);
	*owner = (script_selection_owner){0};
	if (owner == &state->messages && state->queue != NULL)
		script_message_retire(state->queue);
}

static bool selection_current(script_selection_state *state,
	script_selection_owner *owner)
{
	if (owner->window == None || state->display == NULL)
		return false;
	if (XGetSelectionOwner(state->display, owner->selection) ==
	    owner->window)
		return true;
	selection_retire(state, owner, true);
	return false;
}

static Bool selection_clock_event(Display *display, XEvent *event,
	XPointer context)
{
	const script_selection_owner *candidate =
		(const script_selection_owner *)context;

	return event->type == PropertyNotify &&
		event->xproperty.window == candidate->window &&
		event->xproperty.atom == candidate->selection &&
		event->xproperty.state == PropertyNewValue;
}

static bool selection_acquire(script_selection_state *state,
	script_selection_owner *owner, Atom selection, Time time,
	char *text, size_t length)
{
	XSetWindowAttributes attributes = {.event_mask = PropertyChangeMask};
	script_selection_owner candidate = {.selection = selection,
		.text = text, .length = length};

	if (state->display == NULL || state->epoch == UINT64_MAX)
		return false;
	candidate.window = XCreateWindow(state->display,
		DefaultRootWindow(state->display), 0, 0, 1, 1, 0, 0,
		InputOnly, CopyFromParent, CWEventMask, &attributes);
	if (time == CurrentTime)
	{
		XEvent event;
		unsigned char byte = 0;
		script_selection_owner clock = {.window = candidate.window,
			.selection = state->clock_property};

		XChangeProperty(state->display, candidate.window,
			state->clock_property, XA_INTEGER, 8, PropModeReplace,
			&byte, 1);
		/* The round trip queues our preceding real property timestamp.
		 * No input, interpreter or module reader is entered. */
		XSync(state->display, False);
		if (!FCheckIfEvent(state->display, &event,
		    selection_clock_event, (XPointer)&clock) ||
		    event.xproperty.time == CurrentTime)
		{
			XDestroyWindow(state->display, candidate.window);
			return false;
		}
		time = event.xproperty.time;
	}
	XSetSelectionOwner(state->display, selection, candidate.window, time);
	/* A fresh XID distinguishes accepted acquisition from an ignored
	 * stale/future timestamp even when we already own this selection. */
	if (XGetSelectionOwner(state->display, selection) != candidate.window)
	{
		XDestroyWindow(state->display, candidate.window);
		return false;
	}
	candidate.time = time;
	candidate.acquired_ns = selection_now_ns();
	/* Confirmation cannot reenter Script or change this local counter. */
	script_selection_next_epoch(&state->epoch);
	candidate.epoch = state->epoch;
	selection_retire(state, owner, true);
	*owner = candidate;
	return true;
}

bool script_selection_init(script_selection_state *state, Display *display,
	script_message_queue *queue)
{
	char *names[] = {"TARGETS", "TIMESTAMP", "MULTIPLE", "ATOM_PAIR",
		"FVWM_SCRIPT_SELECTION_CLOCK"};
	Atom atoms[5] = {None};

	*state = (script_selection_state){.display = display, .queue = queue};
	if (display == NULL || queue == NULL ||
	    !XInternAtoms(display, names, 5, False, atoms))
		return false;
	state->targets = atoms[0];
	state->timestamp = atoms[1];
	state->multiple = atoms[2];
	state->atom_pair = atoms[3];
	state->clock_property = atoms[4];
	/* Admission opens only after a confirmed private acquisition. */
	script_message_retire(queue);
	return true;
}

bool script_selection_primary_set(script_selection_state *state,
	const char *text, size_t length, Time time)
{
	char *copy;

	if (!script_selection_text_valid(text, length))
		return false;
	copy = fxmalloc(length + 1);
	memcpy(copy, text, length);
	copy[length] = '\0';
	if (selection_acquire(state, &state->primary, XA_PRIMARY, time,
	    copy, length))
		return true;
	free(copy);
	return false;
}

bool script_selection_private_set(script_selection_state *state,
	Atom selection)
{
	if (selection == None || selection == XA_PRIMARY ||
	    state->queue == NULL)
		return false;
	if (!selection_acquire(state, &state->messages, selection,
	    CurrentTime, NULL, 0))
		return false;
	script_message_open(state->queue);
	return true;
}

const char *script_selection_primary_text(script_selection_state *state,
	size_t *length)
{
	*length = 0;
	if (!selection_current(state, &state->primary))
		return NULL;
	*length = state->primary.length;
	return state->primary.text;
}

bool script_selection_private_owned(script_selection_state *state)
{
	return selection_current(state, &state->messages);
}

bool script_selection_clear(script_selection_state *state,
	const XSelectionClearEvent *event)
{
	script_selection_owner *owner = event->selection == XA_PRIMARY ?
		&state->primary : &state->messages;

	if (owner->window == None || event->window != owner->window ||
	    event->selection != owner->selection ||
	    !script_selection_time_valid((uint32_t)event->time,
		(uint32_t)owner->time, owner->acquired_ns, selection_now_ns()))
		return false;
	if (selection_current(state, owner))
		return false;
	return owner == &state->primary;
}

static bool selection_target_add(Atom *targets, size_t *count, Atom target)
{
	for (size_t i = 0; i < *count; i++)
		if (targets[i] == target)
			return true;
	if (*count == SCRIPT_SELECTION_TARGETS)
		return false;
	targets[(*count)++] = target;
	return true;
}

static bool selection_write(script_selection_state *state,
	script_selection_owner *owner, Window requestor, Atom target,
	Atom property, size_t *used)
{
	if (target == state->targets)
	{
		Atom targets[SCRIPT_SELECTION_TARGETS] = {
			state->targets, state->timestamp, state->multiple};
		size_t count = 3;

		if (owner == &state->primary)
			selection_target_add(targets, &count, XA_STRING);
		else
		{
			unsigned long queued[SCRIPT_MESSAGE_LIMIT];
			size_t n = script_message_targets(state->queue, queued,
				SCRIPT_MESSAGE_LIMIT);

			for (size_t i = 0; i < n; i++)
				if (!selection_target_add(targets, &count,
				    queued[i]))
					return false;
		}
		if (!script_selection_charge(used, count * 4))
			return false;
		XChangeProperty(state->display, requestor, property, XA_ATOM,
			32, PropModeReplace, (unsigned char *)targets, count);
		return true;
	}
	if (target == state->timestamp)
	{
		unsigned long time = (uint32_t)owner->time;

		if (!script_selection_charge(used, 4))
			return false;
		XChangeProperty(state->display, requestor, property, XA_INTEGER,
			32, PropModeReplace, (unsigned char *)&time, 1);
		return true;
	}
	if (target == state->multiple || target == None)
		return false;
	if (owner == &state->primary)
	{
		if (target != XA_STRING ||
		    !script_selection_charge(used, owner->length))
			return false;
		XChangeProperty(state->display, requestor, property, XA_STRING,
			8, PropModeReplace, (unsigned char *)owner->text,
			owner->length);
		return true;
	}
	const script_message *next = script_message_peek(state->queue, target);

	if (next == NULL || next->length == 0 ||
	    next->length > SCRIPT_MESSAGE_MAX_BYTES ||
	    !script_selection_charge(used, next->length))
		return false;
	size_t length = next->length;
	char *message = script_message_take(state->queue, target);

	if (message == NULL)
		return false;
	/* Admission is complete. Submission is not a delivery receipt; existing
	 * asynchronous BadWindow policy does not promise reliable requeue. */
	XChangeProperty(state->display, requestor, property, target,
		8, PropModeReplace, (unsigned char *)message, length);
	free(message);
	return true;
}

static bool selection_multiple(script_selection_state *state,
	script_selection_owner *owner, const XSelectionRequestEvent *request)
{
	unsigned char *packed = NULL;
	size_t bytes = 0, used = 0;
	unsigned long pairs[SCRIPT_SELECTION_PAIR_BYTES / 4];
	FPropertyLimits limits = {0, SCRIPT_SELECTION_PAIR_BYTES, 32};

	if (request->property == None ||
	    FPropertyRead(state->display, request->requestor, request->property,
		state->atom_pair, limits, &packed, &bytes) != FPROPERTY_OK)
		return false;
	if (!script_selection_pairs_valid(bytes))
	{
		free(packed);
		return false;
	}
	/* Validate the entire packed list before any property write or take.
	 * Native longs are required only at the XChangeProperty boundary. */
	for (size_t i = 0; i < bytes / 4; i += 2)
	{
		pairs[i] = ((uint32_t *)packed)[i];
		pairs[i + 1] = ((uint32_t *)packed)[i + 1];
		if (!script_selection_pair_valid(pairs[i], pairs[i + 1],
		    state->multiple, request->property) ||
		    !selection_property_valid(state->display, pairs[i + 1]))
			pairs[i + 1] = None;
	}
	free(packed);
	if (!selection_current(state, owner) ||
	    !script_selection_charge(&used, bytes))
		return false;
	for (size_t i = 0; i < bytes / 4; i += 2)
		if (pairs[i + 1] != None && !selection_write(state, owner,
		    request->requestor, pairs[i], pairs[i + 1], &used))
			pairs[i + 1] = None;
	XChangeProperty(state->display, request->requestor, request->property,
		state->atom_pair, 32, PropModeReplace, (unsigned char *)pairs,
		bytes / 4);
	return true;
}

void script_selection_request(script_selection_state *state,
	const XSelectionRequestEvent *request)
{
	script_selection_owner *owner = request->selection == XA_PRIMARY ?
		&state->primary : &state->messages;
	XEvent reply = {.xselection = {.type = SelectionNotify,
		.requestor = request->requestor,
		.selection = request->selection,
		.target = request->target, .time = request->time,
		.property = None}};
	size_t used = 0;
	Atom property = request->property != None ? request->property :
		request->target;

	if (request->requestor == None)
		return;
	if (owner->window != None && request->owner == owner->window &&
	    request->selection == owner->selection && request->target != None &&
	    script_selection_time_valid((uint32_t)request->time,
		(uint32_t)owner->time, owner->acquired_ns,
		selection_now_ns()) &&
	    selection_current(state, owner) &&
	    selection_property_valid(state->display, property))
	{
		bool wrote = request->target == state->multiple ?
			selection_multiple(state, owner, request) :
			selection_write(state, owner, request->requestor,
				request->target, property, &used);

		if (wrote)
			reply.xselection.property = property;
	}
	FSendEvent(state->display, request->requestor, False, 0, &reply);
	XFlush(state->display);
}

void script_selection_release(script_selection_state *state, bool release_x)
{
	selection_retire(state, &state->primary, release_x);
	selection_retire(state, &state->messages, release_x);
}
