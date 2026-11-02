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
#include "safemalloc.h"
#include <X11/Xatom.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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

/* These states belong to one conversion only. Negotiation and fallback share
 * the operation deadline, but never its requestor or partial data. */
typedef enum {
	SELECTION_NOTIFY,
	SELECTION_HEADER_DRAIN,
	SELECTION_CHUNKS
} selection_phase;

typedef struct {
	Display *display;
	int64_t deadline;
	Atom targets;
	Atom utf8;
	Atom incr;
	FSelectionService service;
	void *context;
	Atom selection;
	FSelectionSink sink;
} selection_operation;

typedef struct {
	selection_request request;
	selection_phase phase;
} selection_attempt;

#define SELECTION_TARGETS_MAX_BYTES (256U * 4U)
#define SELECTION_MAX_CHUNKS 4096U
#define SELECTION_EVENT_BATCH 32U

static FSelectionStatus
selection_deadline(const selection_operation *operation)
{
	int64_t now = monotonic_ms();

	if (now < 0)
	{
		return FSELECTION_X_ERROR;
	}
	return now >= operation->deadline ?
	    FSELECTION_TIMED_OUT : FSELECTION_OK;
}

static FSelectionStatus
selection_fence(const selection_operation *operation)
{
	FSelectionStatus status = selection_deadline(operation);

	if (status != FSELECTION_OK || !operation->sink.consume)
		return status;
	if (XGetSelectionOwner(operation->display, operation->selection) !=
		operation->sink.source.owner ||
		!operation->sink.current(operation->display,
			operation->selection, operation->sink.source,
			operation->sink.context))
		return FSELECTION_STALE_SOURCE;
	return selection_deadline(operation);
}

static FSelectionStatus
selection_service(const selection_operation *operation)
{
	FSelectionStatus status = selection_fence(operation);
	bool keep = true;

	if (status != FSELECTION_OK)
	{
		return status;
	}
	if (operation->service != NULL)
	{
		keep = operation->service(operation->context);
	}
	status = selection_fence(operation);
	return status != FSELECTION_OK ? status :
	    (keep ? FSELECTION_OK : FSELECTION_CANCELLED);
}

static Bool
selection_property_matches(Display *display, XEvent *event, XPointer context)
{
	const selection_request *request = (const selection_request *)context;

	return event->type == PropertyNotify &&
	    event->xproperty.window == request->requestor &&
	    event->xproperty.atom == request->property;
}

static Bool
selection_transfer_matches(Display *display, XEvent *event, XPointer context)
{
	const selection_attempt *attempt = (const selection_attempt *)context;

	if (attempt->phase == SELECTION_NOTIFY)
	{
		return selection_matches(display, event,
		    (XPointer)&attempt->request);
	}
	return selection_property_matches(display, event,
	    (XPointer)&attempt->request);
}

static FSelectionStatus
selection_property_status(FPropertyResult result)
{
	switch (result)
	{
	case FPROPERTY_OK:
		return FSELECTION_OK;
	case FPROPERTY_TOO_LARGE:
		return FSELECTION_TOO_LARGE;
	case FPROPERTY_X_ERROR:
		return FSELECTION_X_ERROR;
	case FPROPERTY_CONSUMER_FAILED:
		return FSELECTION_CONSUMER_FAILED;
	default:
		return FSELECTION_INVALID;
	}
}

typedef struct {
	selection_operation *operation;
	size_t base;
	FSelectionStatus status;
} selection_delivery;

static bool
selection_consume(const unsigned char *data, size_t offset, size_t length,
    void *context)
{
	selection_delivery *delivery = context;
	selection_operation *operation = delivery->operation;
	size_t maximum = operation->sink.maximum;

	delivery->status = selection_fence(operation);
	if (delivery->status != FSELECTION_OK)
		return false;
	if (delivery->base > maximum || offset > maximum - delivery->base ||
		length > maximum - delivery->base - offset)
	{
		delivery->status = FSELECTION_TOO_LARGE;
		return false;
	}
	bool consumed = operation->sink.consume(data, delivery->base + offset,
		length, operation->sink.context);

	delivery->status = selection_fence(operation);
	if (delivery->status == FSELECTION_OK && !consumed)
		delivery->status = FSELECTION_CONSUMER_FAILED;
	return delivery->status == FSELECTION_OK;
}

static FSelectionStatus
selection_read_chunk(selection_operation *operation,
    selection_attempt *attempt, Atom expected_type, FPropertyLimits limits,
    size_t base, unsigned char **data, size_t *length)
{
	if (!operation->sink.consume)
		return selection_property_status(FPropertyRead(
			operation->display,
			attempt->request.requestor, attempt->request.property,
			expected_type, limits, data, length));
	selection_delivery delivery = {.operation = operation, .base = base,
		.status = FSELECTION_OK};
	FPropertyResult result = FPropertyConsume8(operation->display,
		attempt->request.requestor, attempt->request.property,
		expected_type, limits, selection_consume, &delivery, length);

	return delivery.status != FSELECTION_OK ? delivery.status :
		selection_property_status(result);
}

/* Probe only metadata, then let FPropertyRead enforce complete bounded reads
 * and LP64 packing. The requestor belongs to us; no global error trap changes
 * the module's existing asynchronous X error policy. */
static FSelectionStatus
selection_read_data(selection_operation *operation, selection_attempt *attempt,
    Atom expected_type, int format, size_t maximum, bool *incremental,
    unsigned char **data, size_t *length)
{
	unsigned char *probe = NULL;
	Atom type = None;
	int actual_format = 0;
	unsigned long count = 0, after = 0;
	const selection_request *request = &attempt->request;
	FPropertyLimits limits = {0, maximum, format};

	*incremental = false;
	*data = NULL;
	*length = 0;
	int rc = XGetWindowProperty(operation->display, request->requestor,
	    request->property, 0, 0, False, AnyPropertyType,
	    &type, &actual_format, &count, &after, &probe);
	if (probe != NULL)
	{
		XFree(probe);
	}
	if (rc != Success)
	{
		return FSELECTION_X_ERROR;
	}
	if (type == operation->incr)
	{
		unsigned char *header = NULL;
		size_t bytes = 0;
		FPropertyLimits header_limits = {4, 4, 32};
		FPropertyResult read = FPropertyRead(operation->display,
		    request->requestor, request->property, operation->incr,
		    header_limits, &header, &bytes);
		FSelectionStatus status = selection_property_status(read);

		if (status == FSELECTION_OK &&
		    ((const uint32_t *)header)[0] > maximum)
		{
			status = FSELECTION_TOO_LARGE;
		}
		free(header);
		*incremental = status == FSELECTION_OK;
		return status;
	}
	return selection_read_chunk(operation, attempt, expected_type, limits,
		0, data, length);
}

static FSelectionStatus
selection_convert(selection_operation *operation, Atom selection, Atom target,
    Atom property, Time time, bool targets, FSelectionResult *result)
{
	FSelectionStatus status = selection_fence(operation);
	unsigned char *data = NULL;
	size_t length = 0;
	Atom expected_type = targets ? XA_ATOM : target;
	int format = targets ? 32 : 8;
	size_t maximum = targets ? SELECTION_TARGETS_MAX_BYTES :
	    operation->sink.consume ? operation->sink.maximum :
	    FSELECTION_TEXT_MAX_BYTES;
	unsigned int chunks = 0, work = 0;
	XEvent event;
	selection_attempt attempt = {
	    .request = {.selection = selection, .target = target,
		.property = property, .time = time},
	    .phase = SELECTION_NOTIFY};
	XSetWindowAttributes attributes = {.event_mask = PropertyChangeMask};

	if (status != FSELECTION_OK)
	{
		return status;
	}
	attempt.request.requestor = XCreateWindow(operation->display,
	    DefaultRootWindow(operation->display), 0, 0, 1, 1, 0, 0,
	    InputOnly, CopyFromParent, CWEventMask, &attributes);
	XConvertSelection(operation->display, selection, target, property,
	    attempt.request.requestor, time);
	XFlush(operation->display);
	struct pollfd descriptor = {
	    ConnectionNumber(operation->display), POLLIN, 0};
	for (;;)
	{
		work = 0;
		while (work < SELECTION_EVENT_BATCH)
		{
			status = selection_fence(operation);
			if (status != FSELECTION_OK)
			{
				goto out;
			}
			if (!FCheckIfEvent(operation->display, &event,
			    selection_transfer_matches, (XPointer)&attempt))
			{
				if (attempt.phase == SELECTION_HEADER_DRAIN)
				{
					/* Retire header NewValue before ACK;
					 * it can precede SelectionNotify.
					 * Only ACK permits the first chunk. */
					XDeleteProperty(operation->display,
					    attempt.request.requestor,
					    property);
					XFlush(operation->display);
					attempt.phase = SELECTION_CHUNKS;
					continue;
				}
				break;
			}
			work++;
			if (attempt.phase == SELECTION_HEADER_DRAIN ||
			    (event.type == PropertyNotify &&
				event.xproperty.state == PropertyDelete))
			{
				/* Consume our own ACK notifications as well. */
				continue;
			}
			if (attempt.phase == SELECTION_NOTIFY)
			{
				if (event.xselection.property == None)
				{
					status = FSELECTION_REFUSED;
					goto out;
				}
				bool incremental = false;

				status = selection_read_data(operation,
				    &attempt, expected_type, format, maximum,
				    &incremental, &data, &length);
				if (status == FSELECTION_OK && incremental)
				{
					/* Fixed role cap, not owner hint. */
					if (!operation->sink.consume)
						data = fxmalloc(maximum + 1);
					attempt.phase = SELECTION_HEADER_DRAIN;
					continue;
				}
				goto out;
			}
			unsigned char *chunk = NULL;
			size_t bytes = 0;
			FPropertyLimits limits = {
			    0, maximum - length, format};
			/* Allow an empty terminal property at the chunk cap,
			 * never a 4097th nonempty delivery. */
			if (operation->sink.consume &&
				chunks == SELECTION_MAX_CHUNKS)
				limits.max_bytes = 0;
			status = selection_read_chunk(operation, &attempt,
				expected_type, limits, length, &chunk, &bytes);
			if (status != FSELECTION_OK)
			{
				free(chunk);
				goto out;
			}
			if (bytes == 0)
			{
				free(chunk);
				if (data)
					data[length] = '\0';
				goto out;
			}
			if (++chunks > SELECTION_MAX_CHUNKS)
			{
				free(chunk);
				status = FSELECTION_TOO_LARGE;
				goto out;
			}
			/* FPropertyRead packs format-32 chunks into aligned
			 * byte lengths; metadata stays in whole atoms. */
			if (!operation->sink.consume)
				memcpy(data + length, chunk, bytes);
			length += bytes;
			free(chunk);
			XDeleteProperty(operation->display,
			    attempt.request.requestor, property);
			XFlush(operation->display);
		}
		status = selection_service(operation);
		if (status != FSELECTION_OK)
		{
			goto out;
		}
		/* A service or synchronous source fence can read our reply into
		 * Xlib while leaving the socket empty. Check only this attempt's
		 * events after the last X request; unrelated input must not spin. */
		if (work == SELECTION_EVENT_BATCH ||
		    FCheckPeekIfEvent(operation->display, &event,
			selection_transfer_matches, (XPointer)&attempt))
		{
			continue;
		}
		int64_t now = monotonic_ms();
		if (now < 0 || now >= operation->deadline)
		{
			status = now < 0 ? FSELECTION_X_ERROR :
			    FSELECTION_TIMED_OUT;
			goto out;
		}
		int remaining = operation->deadline - now;
		if (operation->service != NULL && remaining > 20)
		{
			remaining = 20;
		}
		int ready = poll(&descriptor, 1, remaining);
		if (ready < 0 && errno == EINTR)
		{
			continue;
		}
		if (ready < 0 ||
		    (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)))
		{
			status = FSELECTION_X_ERROR;
			goto out;
		}
	}
out:
	/* Retire the window before filtering: our last ACK notification may
	 * still be in flight. Only this attempt's bounded property traffic is
	 * removed; unrelated input retains FIFO order. XSync has the same
	 * responsive-server requirement as the property reads above. */
	XDestroyWindow(operation->display, attempt.request.requestor);
	XSync(operation->display, False);
	for (unsigned int i = 0; i < SELECTION_EVENT_BATCH; i++)
	{
		if (!FCheckIfEvent(operation->display, &event,
		    selection_property_matches, (XPointer)&attempt.request))
		{
			break;
		}
	}
	if (status == FSELECTION_OK || status == FSELECTION_REFUSED)
	{
		FSelectionStatus deadline = selection_fence(operation);

		if (deadline != FSELECTION_OK)
		{
			status = deadline;
		}
	}
	if (status == FSELECTION_OK)
	{
		result->data = data;
		result->length = length;
		result->type = expected_type;
	}
	else
	{
		free(data);
	}
	return status;
}

static bool
selection_utf8_valid(const unsigned char *data, size_t length)
{
	for (size_t i = 0; i < length;)
	{
		uint32_t value = data[i++];
		unsigned int extra;
		uint32_t minimum;

		if (value < 0x80)
		{
			continue;
		}
		if (value >= 0xc2 && value <= 0xdf)
		{
			extra = 1;
			minimum = 0x80;
			value &= 0x1f;
		}
		else if (value >= 0xe0 && value <= 0xef)
		{
			extra = 2;
			minimum = 0x800;
			value &= 0x0f;
		}
		else if (value >= 0xf0 && value <= 0xf4)
		{
			extra = 3;
			minimum = 0x10000;
			value &= 7;
		}
		else
		{
			return false;
		}
		if (extra > length - i)
		{
			return false;
		}
		for (unsigned int j = 0; j < extra; j++)
		{
			unsigned char next = data[i++];

			if ((next & 0xc0) != 0x80)
			{
				return false;
			}
			value = (value << 6) | (next & 0x3f);
		}
		if (value < minimum || value > 0x10ffff ||
		    (value >= 0xd800 && value <= 0xdfff))
		{
			return false;
		}
	}
	return true;
}

void
FSelectionResultFree(FSelectionResult *result)
{
	if (result != NULL)
	{
		free(result->data);
		*result = (FSelectionResult){0};
	}
}

static FSelectionStatus
selection_request_typed(Display *display, Atom selection, Atom target,
    Atom property, Time time, int timeout_ms, FSelectionService service,
    void *context, FSelectionResult *result, bool text)
{
	int64_t start = monotonic_ms();
	char *names[] = {"TARGETS", "UTF8_STRING", "INCR"};
	Atom atoms[3];
	FSelectionStatus status;

	if (result == NULL)
	{
		return FSELECTION_INVALID;
	}
	*result = (FSelectionResult){0};
	if (display == NULL || selection == None || property == None ||
	    (!text && target == None) || timeout_ms <= 0 ||
	    timeout_ms > FSELECTION_TIMEOUT_MS)
	{
		return FSELECTION_INVALID;
	}
	if (start < 0 || !XInternAtoms(display, names, 3, False, atoms))
	{
		return FSELECTION_X_ERROR;
	}
	selection_operation operation = {.display = display,
	    .deadline = start + timeout_ms, .targets = atoms[0],
	    .utf8 = atoms[1],
	    .incr = atoms[2], .service = service, .context = context};
	if (!text)
	{
		return selection_convert(&operation, selection, target,
		    property,
		    time, false, result);
	}
	FSelectionResult advertised = {0};
	bool utf8 = false, string = false;
	status = selection_convert(&operation, selection, operation.targets,
	    property, time, true, &advertised);
	if (status == FSELECTION_OK)
	{
		for (size_t i = 0; i < advertised.length / 4; i++)
		{
			uint32_t atom = ((const uint32_t *)advertised.data)[i];

			utf8 |= atom == operation.utf8;
			string |= atom == XA_STRING;
		}
		FSelectionResultFree(&advertised);
	}
	else if (status == FSELECTION_REFUSED)
	{
		/* Old owners may lack TARGETS. This is the only path that
		 * guesses an unadvertised representation. */
		utf8 = true;
		string = true;
	}
	else
	{
		return status;
	}
	if (!utf8 && !string)
	{
		return FSELECTION_REFUSED;
	}
	target = utf8 ? operation.utf8 : XA_STRING;
	status = selection_convert(&operation, selection, target, property,
	    time, false, result);
	if (status == FSELECTION_REFUSED && utf8 && string)
	{
		status = selection_convert(&operation, selection, XA_STRING,
		    property, time, false, result);
	}
	if (status == FSELECTION_OK)
	{
		size_t length = result->length;

		if (length != 0 && result->data[length - 1] == '\0')
		{
			length--;
		}
		if (memchr(result->data, '\0', length) != NULL ||
		    (result->type == operation.utf8 &&
			!selection_utf8_valid(result->data, length)))
		{
			FSelectionResultFree(result);
			return FSELECTION_INVALID;
		}
		result->data[length] = '\0';
		result->length = length;
		status = selection_deadline(&operation);
		if (status != FSELECTION_OK)
		{
			FSelectionResultFree(result);
		}
	}
	return status;
}

FSelectionStatus
FSelectionRequestExact(Display *display, Atom selection, Atom target,
    Atom property, Time time, int timeout_ms, FSelectionService service,
    void *context, FSelectionResult *result)
{
	return selection_request_typed(display, selection, target, property,
	    time, timeout_ms, service, context, result, false);
}

FSelectionStatus
FSelectionRequestExactTo(Display *display, Atom selection, Atom target,
    Atom property, Time time, int timeout_ms, FSelectionService service,
    void *service_context, const FSelectionSink *sink,
    FSelectionReceipt *receipt)
{
	int64_t start = monotonic_ms();
	FSelectionResult result = {0};
	selection_operation operation = {.display = display,
		.service = service, .context = service_context,
		.selection = selection};

	if (receipt)
		*receipt = (FSelectionReceipt){0};
	if (!receipt || !sink || !sink->consume || !sink->current ||
		sink->maximum > FSELECTION_TEXT_MAX_BYTES ||
		sink->source.owner == None || !sink->source.epoch ||
		!sink->source.generation ||
		!display || selection == None || target == None ||
		property == None || timeout_ms <= 0 ||
		timeout_ms > FSELECTION_TIMEOUT_MS)
		return FSELECTION_INVALID;
	operation.sink = *sink;
	if (start < 0)
		return FSELECTION_X_ERROR;
	operation.deadline = start + timeout_ms;
	FSelectionStatus status = selection_fence(&operation);

	if (status != FSELECTION_OK)
		return status;
	operation.incr = XInternAtom(display, "INCR", False);
	if (operation.incr == None)
		return FSELECTION_X_ERROR;
	status = selection_convert(&operation, selection, target, property,
		time, false, &result);
	if (status == FSELECTION_OK)
		*receipt = (FSelectionReceipt){
			.length = result.length, .type = result.type
		};
	/* The streaming branch never owns an aggregate, even on failure. */
	FSelectionResultFree(&result);
	return status;
}

FSelectionStatus
FSelectionRequestTextNegotiated(Display *display, Atom selection, Atom property,
    Time time, int timeout_ms, FSelectionService service, void *context,
    FSelectionResult *result)
{
	return selection_request_typed(display, selection, None, property,
	    time, timeout_ms, service, context, result, true);
}
