/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/selection_observer.h"
#include <string.h>
#if defined(HAVE_XFIXES) && HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

enum observer_event_kind { OBSERVER_OTHER, OBSERVER_CHANGED,
	OBSERVER_INVALID, OBSERVER_DESTROYED };
struct observer_collection { bool complete, changed; };

bool selection_observer_compiled(void)
{
#if defined(HAVE_XFIXES) && HAVE_XFIXES
	return true;
#else
	return false;
#endif
}

static enum observer_event_kind observer_kind(
	const struct selection_observer *observer, const XEvent *event)
{
	if (!observer->window || event->xany.display != observer->display ||
		event->xany.send_event)
		return OBSERVER_OTHER;
	if (event->type == DestroyNotify &&
		event->xdestroywindow.window == observer->window)
		return OBSERVER_DESTROYED;
#if defined(HAVE_XFIXES) && HAVE_XFIXES
	if (event->type == observer->event_base + XFixesSelectionNotify)
	{
		XFixesSelectionNotifyEvent change;

		_Static_assert(sizeof(change) <= sizeof(*event),
			"XFixes selection event must fit XEvent");
		memcpy(&change, event, sizeof(change));
		if (change.window != observer->window ||
			change.selection != observer->selection)
			return OBSERVER_OTHER;
		switch (change.subtype)
		{
		case XFixesSetSelectionOwnerNotify:
		case XFixesSelectionWindowDestroyNotify:
		case XFixesSelectionClientCloseNotify:
			return OBSERVER_CHANGED;
		default:
			return OBSERVER_INVALID;
		}
	}
#endif
	return OBSERVER_OTHER;
}

static bool observer_apply(struct selection_observer *observer,
	const XEvent *event, bool update)
{
	enum observer_event_kind kind = observer_kind(observer, event);

	if (kind == OBSERVER_OTHER)
		return false;
	if (kind == OBSERVER_DESTROYED)
	{
		observer->destroyed = true;
		selection_source_close(&observer->policy);
	}
	else if (update)
	{
		if (kind == OBSERVER_CHANGED)
			selection_source_changed(&observer->policy);
		else
			selection_source_uncertain(&observer->policy);
	}
	return true;
}

static struct observer_collection observer_collect(
	struct selection_observer *observer, bool update)
{
	XEvent retained[SELECTION_OBSERVER_EVENT_MAX];
	int queued = XEventsQueued(observer->display, QueuedAlready);
	int kept = 0;
	struct observer_collection result = {
		.complete = queued >= 0 &&
			queued <= SELECTION_OBSERVER_EVENT_MAX
	};

	if (queued < 0)
		queued = 0;
	if (queued > SELECTION_OBSERVER_EVENT_MAX)
		queued = SELECTION_OBSERVER_EVENT_MAX;
	/* Like FEvent's bounded snapshot/restore helpers: each raw next-event
	 * has a queued record, never waits or changes input caches. */
	for (int i = 0; i < queued; i++)
	{
		XEvent event;

		XNextEvent(observer->display, &event);
		if (observer_apply(observer, &event, update))
			result.changed = true;
		else
			retained[kept++] = event;
	}
	while (kept)
		XPutBackEvent(observer->display, &retained[--kept]);
	if (update && !result.complete)
		selection_source_uncertain(&observer->policy);
	return result;
}

static enum selection_source_result observer_status(
	const struct selection_observer *observer)
{
	struct selection_source_token ignored;

	return selection_source_token_get(&observer->policy, &ignored);
}

enum selection_source_result selection_observer_refresh(
	struct selection_observer *observer, FSelectionSource *source)
{
	if (source)
		*source = (FSelectionSource){0};
	if (!source || !observer || !observer->policy.initialized)
		return SELECTION_SOURCE_INVALID;
	if (observer->busy)
		return SELECTION_SOURCE_BUSY;
	enum selection_source_result status = observer_status(observer);

	if (status != SELECTION_SOURCE_OK && status != SELECTION_SOURCE_EMPTY &&
		status != SELECTION_SOURCE_RETRY)
		return status;
	observer->busy = true;
	selection_source_uncertain(&observer->policy);
	Window owner = XGetSelectionOwner(observer->display,
		observer->selection);
	struct observer_collection collection =
		observer_collect(observer, true);
	struct selection_source_token token;

	if (owner > UINT32_MAX)
	{
		status = SELECTION_SOURCE_ERROR;
		goto out;
	}
	status = selection_source_snapshot(&observer->policy, (uint32_t)owner,
		collection.complete && !collection.changed, &token);
	if (status == SELECTION_SOURCE_OK)
		*source = (FSelectionSource){.owner = token.owner,
			.epoch = token.epoch, .generation = token.generation};
out:
	observer->busy = false;
	return status;
}

enum selection_source_result selection_observer_open(
	struct selection_observer *observer, Display *display, Atom selection,
	uint64_t epoch, FSelectionSource *source)
{
	if (source)
		*source = (FSelectionSource){0};
	if (!observer || !source || !display || selection == None || !epoch)
		return SELECTION_SOURCE_INVALID;
	if (observer->busy || (observer->policy.initialized &&
		(observer->policy.phase != SELECTION_SOURCE_PHASE_CLOSED ||
		 observer->window != None)))
		return SELECTION_SOURCE_BUSY;
	if (observer->policy.initialized && epoch == observer->policy.epoch)
		return SELECTION_SOURCE_INVALID;
	*observer = (struct selection_observer){0};
	observer->display = display;
	observer->selection = selection;
	selection_source_init(&observer->policy, epoch, false);
#if defined(HAVE_XFIXES) && HAVE_XFIXES
	int major = 1, minor = 0, error_base = 0;

	if (!XFixesQueryExtension(display, &observer->event_base,
		&error_base) ||
		!XFixesQueryVersion(display, &major, &minor) || major < 1 ||
		observer->event_base < LASTEvent || observer->event_base > 127)
		return SELECTION_SOURCE_UNAVAILABLE;
	selection_source_init(&observer->policy, epoch, true);
	XSetWindowAttributes attributes = {.event_mask = StructureNotifyMask};

	observer->window = XCreateWindow(display, DefaultRootWindow(display),
		0, 0, 1, 1, 0, 0, InputOnly, CopyFromParent, CWEventMask,
		&attributes);
	if (observer->window == None)
	{
		selection_source_close(&observer->policy);
		return SELECTION_SOURCE_ERROR;
	}
	XFixesSelectSelectionInput(display, observer->window, selection,
		XFixesSetSelectionOwnerNotifyMask |
		XFixesSelectionWindowDestroyNotifyMask |
		XFixesSelectionClientCloseNotifyMask);
	return selection_observer_refresh(observer, source);
#else
	return SELECTION_SOURCE_UNAVAILABLE;
#endif
}

enum selection_source_result selection_observer_event(
	struct selection_observer *observer, const XEvent *event,
	bool *consumed)
{
	if (consumed)
		*consumed = false;
	if (!consumed || !observer || !event || !observer->policy.initialized)
		return SELECTION_SOURCE_INVALID;
	if (observer->busy)
		return SELECTION_SOURCE_BUSY;
	observer->busy = true;
	*consumed = observer_apply(observer, event, true);
	observer->busy = false;
	return observer_status(observer);
}

bool selection_observer_current(Display *display, Atom selection,
	FSelectionSource source, void *context)
{
	struct selection_observer *observer = context;

	if (!observer || display != observer->display ||
		selection != observer->selection || observer->busy ||
		source.owner > UINT32_MAX)
		return false;
	struct selection_source_token token = {.owner = (uint32_t)source.owner,
		.epoch = source.epoch, .generation = source.generation};

	if (!selection_source_current(&observer->policy, token))
		return false;
	observer->busy = true;
	struct observer_collection collection =
		observer_collect(observer, true);
	bool current = collection.complete && !collection.changed &&
		selection_source_current(&observer->policy, token);

	observer->busy = false;
	return current;
}

enum selection_source_result selection_observer_close(
	struct selection_observer *observer)
{
	if (!observer || !observer->policy.initialized)
		return SELECTION_SOURCE_INVALID;
	if (observer->busy)
		return SELECTION_SOURCE_BUSY;
	observer->busy = true;
	selection_source_close(&observer->policy);
	if (observer->window == None)
	{
		observer->busy = false;
		return SELECTION_SOURCE_CLOSED;
	}
	if (!observer->destroyed)
	{
		observer->destroyed = true;
		XDestroyWindow(observer->display, observer->window);
		XSync(observer->display, False);
	}
	struct observer_collection collection =
		observer_collect(observer, false);

	if (collection.complete)
		observer->window = None;
	observer->busy = false;
	return collection.complete ? SELECTION_SOURCE_CLOSED :
		SELECTION_SOURCE_RETRY;
}
