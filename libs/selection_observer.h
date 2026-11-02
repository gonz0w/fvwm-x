#ifndef FVWM_SELECTION_OBSERVER_H
#define FVWM_SELECTION_OBSERVER_H

#include "libs/FSelection.h"
#include "libs/selection_source_policy.h"

#define SELECTION_OBSERVER_EVENT_MAX 32

/* One serialized module-only observer, fixed through all pending callbacks.
 * Zero-initialize fresh storage; supply a unique nonzero lifetime epoch.
 * Restart only after close completes, with a different caller-unique epoch.
 * Keep display open and metadata alive until callbacks and close finish.
 * Do not mutate fields, destroy its window independently or change binding.
 * No global handler, selection ownership or mandatory XFixes link changes. */
struct selection_observer {
	Display *display;
	Atom selection;
	Window window;
	struct selection_source_policy policy;
	int event_base;
	bool busy, destroyed;
};

bool selection_observer_compiled(void);
enum selection_source_result selection_observer_open(
	struct selection_observer *observer, Display *display, Atom selection,
	uint64_t epoch, FSelectionSource *source);
/* One explicit query/drain attempt, outside any entered transfer callback.
 * RETRY keeps the subscription but issues no token. Caller services unrelated
 * queue progress before retrying; never loop internally to stabilize a flood.
 * Only a complete scan with no ownership event publishes a fresh snapshot. */
enum selection_source_result selection_observer_refresh(
	struct selection_observer *observer, FSelectionSource *source);
/* Exact server observer events only. Other records remain caller-owned.
 * Never discard a successor/loss event because its owner differs. */
enum selection_source_result selection_observer_event(
	struct selection_observer *observer, const XEvent *event,
	bool *consumed);
/* Fits FSelectionSourceCurrent. Caller has just queried owner on the same
 * connection. This callback only scans already-queued records: no X request,
 * general event pump or replacement token. Unsynchronized/closed state refuses
 * without rescanning. Restore every unrelated/requestor event in FIFO order. */
bool selection_observer_current(Display *display, Atom selection,
	FSelectionSource source, void *context);
/* Invalidate before destroying only the private observer window. RETRY means
 * bounded queue cleanup remains; preserve metadata and let the outer dispatcher
 * make progress before another close call. Do not free callbacks' context.
 * The caller's existing asynchronous X policy handles expected private-window
 * destruction races; this helper never replaces a display/global handler.
 * A responsive server is required; entered Xlib calls are not preempted. */
enum selection_source_result selection_observer_close(
	struct selection_observer *observer);

#endif
