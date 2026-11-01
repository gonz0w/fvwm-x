/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/script_selection.h"
#include "libs/FEvent.h"
#include "libs/FProperty.h"
#include "libs/safemalloc.h"
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool allow_bad_window;
static Window departed_window;
static unsigned int bad_windows;

static int expected_error(Display *display, XErrorEvent *error)
{
	(void)display;
	if (allow_bad_window && error->error_code == BadWindow &&
	    error->resourceid == departed_window &&
	    (error->request_code == X_ChangeProperty ||
	     error->request_code == X_SendEvent))
	{
		bad_windows++;
		return 0;
	}
	fprintf(stderr, "unexpected X error %u request %u\n",
		error->error_code, error->request_code);
	abort();
}

static int64_t now_ms(void)
{
	struct timespec now;

	assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static XEvent wait_event(Display *display, int type)
{
	int64_t deadline = now_ms() + 1000;
	XEvent event;

	while (!FCheckTypedEvent(display, type, &event))
	{
		int64_t remaining = deadline - now_ms();
		struct pollfd ready = {ConnectionNumber(display), POLLIN, 0};

		assert(remaining > 0);
		int result = poll(&ready, 1, remaining);

		assert(result >= 0 || errno == EINTR);
		assert(!(ready.revents & (POLLERR | POLLHUP | POLLNVAL)));
	}
	return event;
}

static XSelectionRequestEvent request(Display *peer, Window window,
	Atom selection, Atom target, Atom property, Display *owner)
{
	XConvertSelection(peer, selection, target, property, window,
		CurrentTime);
	XFlush(peer);
	XEvent event = wait_event(owner, SelectionRequest);

	assert(event.xselectionrequest.selection == selection);
	assert(event.xselectionrequest.target == target);
	assert(event.xselectionrequest.requestor == window);
	assert(event.xselectionrequest.property == property);
	return event.xselectionrequest;
}

static void receive(script_selection_state *state, Display *peer,
	const XSelectionRequestEvent *requested, const void *expected,
	size_t length)
{
	unsigned char *data = NULL;
	size_t bytes = 0;
	FPropertyLimits limits = {0, SCRIPT_SELECTION_MAX_BYTES, 8};

	script_selection_request(state, requested);
	XEvent reply = wait_event(peer, SelectionNotify);

	assert(reply.xselection.requestor == requested->requestor);
	assert(reply.xselection.selection == requested->selection);
	assert(reply.xselection.target == requested->target);
	assert(reply.xselection.time == requested->time);
	assert(reply.xselection.property == requested->property);
	assert(FPropertyRead(peer, requested->requestor, requested->property,
		requested->target, limits, &data, &bytes) == FPROPERTY_OK);
	assert(bytes == length && memcmp(data, expected, length) == 0);
	free(data);
}

int main(void)
{
	Display *owner = XOpenDisplay(NULL), *peer = XOpenDisplay(NULL);
	script_message_queue queue = {0};
	script_selection_state state;
	XWindowAttributes attributes;
	size_t length = 0;

	assert(owner != NULL && peer != NULL);
	XSetErrorHandler(expected_error);
	Window window = XCreateSimpleWindow(peer, DefaultRootWindow(peer),
		0, 0, 1, 1, 0, 0, 0);
	Atom property = XInternAtom(peer, "SCRIPT_OWNER_API_RESULT", False);
	Atom private = XInternAtom(peer, "SCRIPT_OWNER_API_PRIVATE", False);
	Atom target = XInternAtom(peer, "SCRIPT_OWNER_API_RECEIVER", False);

	assert(script_selection_init(&state, owner, &queue));
	assert(queue.retired);
	assert(script_selection_primary_set(&state, "snapshot", 8,
		CurrentTime));
	Window first = state.primary.window;
	uint64_t epoch = state.primary.epoch;
	Time acquired = state.primary.time;
	uint32_t stale = (uint32_t)acquired - 1;
	uint32_t future = (uint32_t)acquired + UINT32_C(0x40000000);

	assert(acquired != CurrentTime);
	if (stale == CurrentTime)
		stale = (uint32_t)acquired - 2;
	if (future == CurrentTime)
		future++;
	assert(XGetWindowAttributes(owner, first, &attributes));
	assert(attributes.class == InputOnly);
	assert(!script_selection_primary_set(&state, "stale", 5, stale));
	assert(!script_selection_primary_set(&state, "future", 6, future));
	assert(state.primary.window == first && state.primary.epoch == epoch);
	assert(strcmp(script_selection_primary_text(&state, &length),
		"snapshot") == 0 && length == 8);
	puts("owner stale/future acquisition preserves confirmed snapshot");

	char *maximum = fxmalloc(SCRIPT_SELECTION_MAX_BYTES + 1);

	memset(maximum, 'x', SCRIPT_SELECTION_MAX_BYTES + 1);
	assert(script_selection_primary_set(&state, maximum,
		SCRIPT_SELECTION_MAX_BYTES, CurrentTime));
	assert(state.primary.window != first && state.primary.epoch > epoch);
	first = state.primary.window;
	assert(!script_selection_primary_set(&state, maximum,
		SCRIPT_SELECTION_MAX_BYTES + 1, CurrentTime));
	assert(state.primary.window == first);
	XSelectionRequestEvent pending = request(peer, window, XA_PRIMARY,
		XA_STRING, property, owner);

	receive(&state, peer, &pending, maximum, SCRIPT_SELECTION_MAX_BYTES);
	free(maximum);
	puts("owner exact wire cap and cap+1 refusal preserve full bytes");

	assert(script_selection_private_set(&state, private) && !queue.retired);
	assert(script_message_enqueue(&queue, target, fxstrdup("departed")));
	assert(script_message_enqueue(&queue, target, fxstrdup("survivor")));
	pending = request(peer, window, private, target, property, owner);
	XDestroyWindow(peer, window);
	XSync(peer, False);
	departed_window = window;
	allow_bad_window = true;
	script_selection_request(&state, &pending);
	XSync(owner, False);
	allow_bad_window = false;
	assert(bad_windows >= 1 && queue.count == 1);
	window = XCreateSimpleWindow(peer, DefaultRootWindow(peer),
		0, 0, 1, 1, 0, 0, 0);
	pending = request(peer, window, private, target, property, owner);
	receive(&state, peer, &pending, "survivor", sizeof("survivor"));
	assert(queue.count == 0);
	puts("vanished recipient preserves later delivery "
		"without requeue claim");

	XSetSelectionOwner(peer, XA_PRIMARY, window, CurrentTime);
	XSetSelectionOwner(peer, private, window, CurrentTime);
	XSync(peer, False);
	assert(script_selection_primary_text(&state, &length) == NULL);
	assert(length == 0 && !script_selection_private_owned(&state));
	assert(queue.retired);
	script_selection_release(&state, true);
	XSync(owner, False);
	assert(XGetSelectionOwner(peer, XA_PRIMARY) == window);
	assert(XGetSelectionOwner(peer, private) == window);
	puts("ownership loss and teardown preserve newer foreign selections");
	XDestroyWindow(peer, window);
	XCloseDisplay(peer);
	XCloseDisplay(owner);
	return 0;
}
