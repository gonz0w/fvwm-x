/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "FSelection.h"
#include "FEvent.h"
#include "FProperty.h"
#include "log.h"
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
	Window requestor;
	Atom selection;
	Atom target;
	Atom property;
	Time time;
} selection_request;

static Bool
selection_matches(Display *display, XEvent *event, XPointer context)
{
	const selection_request *request = (const selection_request *)context;
	const XSelectionEvent *reply = &event->xselection;

	return event->type == SelectionNotify &&
	    reply->requestor == request->requestor &&
	    reply->selection == request->selection &&
	    reply->target == request->target &&
	    reply->time == request->time &&
	    (reply->property == request->property || reply->property == None);
}

static int64_t
monotonic_ms(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
	{
		return -1;
	}
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

char *
FSelectionRequestTextWithService(Display *display, Atom selection, Atom target,
    Atom property, Time time, int timeout_ms, FSelectionService service,
    void *context)
{
	unsigned char *data = NULL;
	size_t bytes = 0;
	XEvent event;
	int64_t start = monotonic_ms();
	FPropertyLimits limits = {0, FSELECTION_TEXT_MAX_BYTES, 8};

	if (selection == None || target == None || property == None ||
	    timeout_ms <= 0 || timeout_ms > FSELECTION_TIMEOUT_MS || start < 0)
	{
		return NULL;
	}
	/* Never reuse a timed-out requestor: a delayed owner must not supply
	 * stale data to a subsequent paste or script-message request. */
	selection_request request = {
	    .requestor = XCreateWindow(display, DefaultRootWindow(display),
		0, 0, 1, 1, 0, 0, InputOnly, CopyFromParent, 0, NULL),
	    .selection = selection, .target = target, .property = property,
	    .time = time};
	XConvertSelection(display, selection, target, property,
	    request.requestor, time);
	XFlush(display);
	struct pollfd descriptor = {ConnectionNumber(display), POLLIN, 0};
	for (;;)
	{
		if (service != NULL && !service(context))
			break;
		int64_t now = monotonic_ms();
		if (now < 0 || now - start >= timeout_ms)
		{
			fvwm_debug(__func__, "Selection request timed out");
			break;
		}
		if (FCheckIfEvent(display, &event, selection_matches,
		    (XPointer)&request))
		{
			if (event.xselection.property != None &&
			    FPropertyRead(display, request.requestor,
				property, target,
				limits, &data, &bytes) != FPROPERTY_OK)
			{
				fvwm_debug(__func__,
				    "Ignoring invalid selection text");
			}
			break;
		}
		/* poll avoids fd_set limits and sleeps for a silent owner.
		 * Recompute the absolute deadline after every EINTR. */
		now = monotonic_ms();
		if (now < 0 || now - start >= timeout_ms)
		{
			break;
		}
		int remaining = timeout_ms - (now - start);
		if (service != NULL && remaining > 20)
			remaining = 20;
		int ready = poll(&descriptor, 1, remaining);
		if (ready < 0 && errno == EINTR)
		{
			continue;
		}
		if (ready < 0 ||
		    (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)))
		{
			break;
		}
	}
	XDestroyWindow(display, request.requestor);
	XFlush(display);
	return (char *)data;
}

char *
FSelectionRequestText(Display *display, Atom selection, Atom target,
    Atom property, Time time, int timeout_ms)
{
	return FSelectionRequestTextWithService(display, selection, target,
	    property, time, timeout_ms, NULL, NULL);
}
