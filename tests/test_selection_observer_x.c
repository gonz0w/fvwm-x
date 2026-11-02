/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/selection_observer.h"
#include <X11/Xatom.h>
#include <assert.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(HAVE_XFIXES) && HAVE_XFIXES
#include <X11/extensions/Xfixes.h>

static int64_t milliseconds(void)
{
	struct timespec now;

	assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static FSelectionSource stabilize(struct selection_observer *observer)
{
	FSelectionSource source;
	enum selection_source_result status = SELECTION_SOURCE_RETRY;

	for (unsigned i = 0; i < 3 && status == SELECTION_SOURCE_RETRY; i++)
		status = selection_observer_refresh(observer, &source);
	assert(status == SELECTION_SOURCE_OK);
	assert(source.epoch && source.generation && source.owner);
	return source;
}

static void set_owner(Display *display, Atom selection, Window owner)
{
	XSetSelectionOwner(display, selection, owner, CurrentTime);
	XSync(display, False);
	assert(XGetSelectionOwner(display, selection) == owner);
}

static bool current(struct selection_observer *observer,
	FSelectionSource source)
{
	/* Required same-connection barrier before queued-only checking. */
	XGetSelectionOwner(observer->display, observer->selection);
	return selection_observer_current(observer->display,
		observer->selection,
		source, observer);
}

static void close_observer(struct selection_observer *observer)
{
	assert(selection_observer_close(observer) == SELECTION_SOURCE_CLOSED);
	assert(selection_observer_close(observer) == SELECTION_SOURCE_CLOSED);
	assert(observer->window == None);
}

static void filter_and_fifo(struct selection_observer *observer,
	Display *owner, Atom other, Window first)
{
	FSelectionSource source = stabilize(observer);

	set_owner(owner, other, first);
	assert(XGetSelectionOwner(owner, observer->selection) == source.owner);
	XEvent marker = {.xexpose = {.type = Expose,
		.display = observer->display, .window = first}};

	for (int i = 7; i >= 0; i--)
	{
		marker.xexpose.count = i;
		XPutBackEvent(observer->display, &marker);
	}
	int queued = XEventsQueued(observer->display, QueuedAlready);
	uint64_t generation = observer->policy.generation;

	/* Identical numeric token cannot authorize a different query binding.
	 * Refusal must not touch this observer's policy or queued records. */
	assert(!selection_observer_current(owner, observer->selection,
		source, observer));
	assert(!selection_observer_current(observer->display, other,
		source, observer));
	FSelectionSource old = source;

	old.epoch++;
	assert(!selection_observer_current(observer->display,
		observer->selection,
		old, observer));
	assert(observer->policy.generation == generation &&
		XEventsQueued(observer->display, QueuedAlready) == queued);
	assert(current(observer, source));
	for (int i = 0; i < 8; i++)
	{
		XNextEvent(observer->display, &marker);
		assert(marker.type == Expose && marker.xexpose.count == i);
	}
	/* Synthetic C records check the filter, not server behavior. */
	XFixesSelectionNotifyEvent change = {
		.type = observer->event_base + XFixesSelectionNotify,
		.display = observer->display, .window = observer->window,
		.selection = observer->selection, .owner = first,
		.subtype = XFixesSetSelectionOwnerNotify,
		.timestamp = 0xffffffffUL, .selection_timestamp = 0xffffffffUL,
		.send_event = True
	};
	XEvent event = {0};
	bool consumed = true;

	memcpy(&event, &change, sizeof(change));
	assert(selection_observer_event(observer, &event, &consumed) ==
		SELECTION_SOURCE_OK && !consumed);
	assert(current(observer, source));
	change.send_event = False;
	change.subtype = 99;
	memcpy(&event, &change, sizeof(change));
	assert(selection_observer_event(observer, &event, &consumed) ==
		SELECTION_SOURCE_RETRY && consumed);
	assert(!current(observer, source));
	source = stabilize(observer);
	/* Fill the unrelated prefix beyond the fixed scan budget. */
	for (int i = SELECTION_OBSERVER_EVENT_MAX + 3; i >= 0; i--)
	{
		marker.xexpose.count = i;
		XPutBackEvent(observer->display, &marker);
	}
	assert(!current(observer, source));
	queued = XEventsQueued(observer->display, QueuedAlready);
	generation = observer->policy.generation;
	assert(!current(observer, source));
	assert(XEventsQueued(observer->display, QueuedAlready) == queued &&
		observer->policy.generation == generation);
	for (int i = 0; i <= SELECTION_OBSERVER_EVENT_MAX + 3; i++)
	{
		XNextEvent(observer->display, &marker);
		assert(marker.type == Expose && marker.xexpose.count == i);
	}
	source = stabilize(observer);
	observer->busy = true; /* Test-only recursive-entry setup. */
	FSelectionSource refused = source;

	assert(selection_observer_refresh(observer, &refused) ==
		SELECTION_SOURCE_BUSY);
	assert(!refused.epoch && !refused.owner && !refused.generation);
	assert(!current(observer, source));
	assert(selection_observer_close(observer) == SELECTION_SOURCE_BUSY);
	observer->busy = false;
	assert(current(observer, source));
}

struct transfer {
	struct selection_observer *observer;
	Display *owner;
	Window window;
	bool reacquire, served;
	size_t length;
};

static bool transfer_current(Display *display, Atom selection,
	FSelectionSource source, void *context)
{
	struct transfer *transfer = context;

	return selection_observer_current(display, selection, source,
		transfer->observer);
}

static bool transfer_consume(const unsigned char *data, size_t offset,
	size_t length, void *context)
{
	struct transfer *transfer = context;

	assert(offset == transfer->length && offset + length <= 5);
	for (size_t i = 0; i < length; i++)
		assert(data[i] == (unsigned char)(offset + i));
	transfer->length += length;
	return true;
}

static bool transfer_service(void *context)
{
	struct transfer *transfer = context;

	for (unsigned i = 0; i < 8 && XPending(transfer->owner); i++)
	{
		XEvent event;

		XNextEvent(transfer->owner, &event);
		if (event.type != SelectionRequest)
			continue;
		XSelectionRequestEvent request = event.xselectionrequest;
		const unsigned char bytes[] = {0, 1, 2, 3, 4};

		assert(!transfer->served && request.owner == transfer->window);
		XChangeProperty(transfer->owner, request.requestor,
			request.property, request.target, 8, PropModeReplace,
			bytes, sizeof(bytes));
		event = (XEvent){.xselection = {.type = SelectionNotify,
			.requestor = request.requestor,
			.selection = request.selection,
			.target = request.target, .property = request.property,
			.time = request.time}};
		assert(XSendEvent(transfer->owner, request.requestor, False,
			0, &event));
		if (transfer->reacquire)
			XSetSelectionOwner(transfer->owner, request.selection,
				transfer->window, CurrentTime);
		XSync(transfer->owner, False);
		transfer->served = true;
	}
	return true;
}

static void transfer_tests(struct selection_observer *observer,
	Display *owner, Window window, Atom property)
{
	for (unsigned scenario = 0; scenario < 2; scenario++)
	{
		FSelectionSource source = stabilize(observer);
		struct transfer transfer = {
			.observer = observer, .owner = owner,
			.window = window, .reacquire = scenario == 1};
		FSelectionSink sink = {.source = source, .maximum = 5,
			.current = transfer_current,
			.consume = transfer_consume,
			.context = &transfer};
		FSelectionReceipt receipt;
		FSelectionStatus status = FSelectionRequestExactTo(
			observer->display,
			observer->selection, XA_STRING, property,
			CurrentTime, 1000,
			transfer_service, &transfer, &sink, &receipt);

		assert(transfer.served);
		if (!scenario)
			assert(status == FSELECTION_OK && receipt.length == 5 &&
				transfer.length == 5);
		else
			assert(status == FSELECTION_STALE_SOURCE &&
				!receipt.length &&
				!transfer.length);
	}
}

static void enabled_tests(Display *display, Atom selection, Atom property)
{
	Display *owner = XOpenDisplay(NULL);

	assert(owner);
	Window first = XCreateSimpleWindow(owner, DefaultRootWindow(owner),
		0, 0, 1, 1, 0, 0, 0);
	Window second = XCreateSimpleWindow(owner, DefaultRootWindow(owner),
		0, 0, 1, 1, 0, 0, 0);
	Atom other = XInternAtom(display, "FVWM_OBSERVER_OTHER", False);
	struct selection_observer observer = {0};
	FSelectionSource source;

	set_owner(owner, selection, first);
	enum selection_source_result opened = selection_observer_open(&observer,
		display, selection, 71, &source);

	if (opened == SELECTION_SOURCE_UNAVAILABLE)
	{
		XCloseDisplay(owner);
		XCloseDisplay(display);
		puts("XFixes unavailable; observer qualification refused");
		exit(77);
	}
	assert(opened == SELECTION_SOURCE_OK ||
		opened == SELECTION_SOURCE_RETRY);
	source = stabilize(&observer);
	FSelectionSource original = source;
	Window observer_window = observer.window;

	assert(selection_observer_open(&observer, display, selection, 72,
		&source) == SELECTION_SOURCE_BUSY);
	assert(observer.window == observer_window && !source.epoch);
	source = original;
	/* Actual same-window reacquisition. */
	set_owner(owner, selection, first);
	assert(XGetSelectionOwner(display, selection) == first);
	assert(!selection_observer_current(display, selection,
		source, &observer));
	assert(observer.policy.generation > source.generation);
	printf("{\"same_window\":true,\"owner\":%lu,"
		"\"old_generation\":%llu,\"observed_generation\":%llu}\n",
		first, (unsigned long long)source.generation,
		(unsigned long long)observer.policy.generation);
	source = stabilize(&observer);
	assert(source.generation > original.generation);
	set_owner(owner, selection, second);
	set_owner(owner, selection, first);
	assert(XGetSelectionOwner(display, selection) == first);
	assert(!selection_observer_current(display, selection,
		source, &observer));
	assert(observer.policy.generation > source.generation);
	printf("{\"owner_aba\":true,\"owner\":%lu,"
		"\"old_generation\":%llu,\"observed_generation\":%llu}\n",
		first, (unsigned long long)source.generation,
		(unsigned long long)observer.policy.generation);
	source = stabilize(&observer);
	/* A refresh query must drain a real pending change and refuse its
	 * own snapshot, even without an intervening current/event call. */
	uint64_t before_refresh = observer.policy.generation;

	set_owner(owner, selection, first);
	assert(selection_observer_refresh(&observer, &source) ==
		SELECTION_SOURCE_RETRY);
	assert(!source.owner && !source.epoch && !source.generation);
	assert(observer.policy.generation > before_refresh);
	source = stabilize(&observer);
	filter_and_fifo(&observer, owner, other, first);
	transfer_tests(&observer, owner, first, property);
	source = stabilize(&observer);
	XDestroyWindow(owner, first);
	XSync(owner, False);
	assert(!current(&observer, source));
	assert(selection_observer_refresh(&observer, &source) ==
		SELECTION_SOURCE_EMPTY);
	assert(!source.epoch && !source.owner);
	set_owner(owner, selection, second);
	source = stabilize(&observer);
	XCloseDisplay(owner); /* Independent owner connection disappears. */
	int64_t deadline = milliseconds() + 1000;

	while (XGetSelectionOwner(display, selection) != None)
	{
		assert(milliseconds() < deadline);
		assert(poll(NULL, 0, 1) == 0);
	}
	assert(!current(&observer, source));
	assert(selection_observer_refresh(&observer, &source) ==
		SELECTION_SOURCE_EMPTY);
	close_observer(&observer);
	assert(selection_observer_open(&observer, display, selection, 71,
		&source) == SELECTION_SOURCE_INVALID);
	opened = selection_observer_open(&observer, display, selection, 72,
		&source);
	assert(opened == SELECTION_SOURCE_EMPTY ||
		opened == SELECTION_SOURCE_RETRY);
	assert(!selection_observer_current(display, selection,
		original, &observer));
	/* External destruction invalidates before any new token. */
	Window destroyed = observer.window;

	XDestroyWindow(display, destroyed);
	XSync(display, False);
	assert(selection_observer_refresh(&observer, &source) ==
		SELECTION_SOURCE_CLOSED);
	assert(observer.destroyed);
	close_observer(&observer);
}
#endif

int main(void)
{
	Display *display = XOpenDisplay(NULL);

	assert(display);
	Atom selection = XInternAtom(display, "FVWM_OBSERVER_SELECTION", False);
#if defined(HAVE_XFIXES) && HAVE_XFIXES
	Atom property = XInternAtom(display, "FVWM_OBSERVER_DATA", False);
#endif

	if (!selection_observer_compiled())
	{
		struct selection_observer observer = {0};
		FSelectionSource source = {.epoch = 99};

		assert(selection_observer_open(&observer, display,
			selection, 91,
			&source) == SELECTION_SOURCE_UNAVAILABLE);
		assert(!source.epoch && !source.owner && !source.generation);
		assert(selection_observer_close(&observer) ==
			SELECTION_SOURCE_CLOSED);
		puts("observer build unavailable; refusal verified only");
	}
	else
	{
#if defined(HAVE_XFIXES) && HAVE_XFIXES
		enabled_tests(display, selection, property);
		puts("ordered observer public-X cases passed");
#endif
	}
	XCloseDisplay(display);
	return 0;
}
