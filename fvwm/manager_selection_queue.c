/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "manager_selection.h"
#include <unistd.h>

void manager_selection_interaction(manager_selection *state)
{
	XEvent retained[MANAGER_SELECTION_EVENT_MAX];
	size_t kept = 0;

	if (state->display == NULL || state->process != getpid() ||
	    !state->active || state->closing || state->loss_pending)
	{
		return;
	}
	int queued = XEventsQueued(state->display, QueuedAlready);
	int count = queued > (int)MANAGER_SELECTION_EVENT_MAX ?
		(int)MANAGER_SELECTION_EVENT_MAX : queued;

	/* Raw operations inspect only this already-queued prefix. They cannot
	 * wait for a record or change FEvent's cached input/time. Retained input
	 * and lifecycle records go back in exactly their original order. */
	for (int i = 0; i < count; i++)
	{
		XEvent event;

		XNextEvent(state->display, &event);
		if (event.type == SelectionClear &&
		    manager_selection_defer_clear(state, &event.xselectionclear))
		{
			retained[kept++] = event;
		}
		else if (!state->loss_pending &&
		    manager_selection_event(state, &event))
		{
			/* Complete preceding markers before an ensuing clear, but
			 * never dispatch commands or release window resources here. */
			manager_selection_service(state);
		}
		else
		{
			retained[kept++] = event;
		}
	}
	while (kept > 0)
		XPutBackEvent(state->display, &retained[--kept]);
	/* A marker already extracted by a modal mask has unknown position
	 * relative to the remaining queue. An incomplete scan cannot establish
	 * that no genuine ownership loss precedes it. Refusal remains finite;
	 * the tombstone waits for ordinary dispatch rather than skipping loss. */
	if (queued >= 0 && queued <= (int)MANAGER_SELECTION_EVENT_MAX &&
	    !state->loss_pending && state->deferred_marker_valid)
	{
		XEvent event = state->deferred_marker;

		state->deferred_marker_valid = false;
		(void)manager_selection_event(state, &event);
		manager_selection_service(state);
	}
}

void manager_selection_ordinary(manager_selection *state)
{
	/* A modal mask may already have removed the marker from Xlib's queue.
	 * Returning to ordinary dispatch must reconcile it without requiring
	 * another interaction. The same bounded scan preserves loss order. */
	if (state->deferred_marker_valid)
		manager_selection_interaction(state);
	manager_selection_service(state);
}
