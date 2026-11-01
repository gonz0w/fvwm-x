/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/manager_selection.h"
#include "libs/FEvent.h"
#include "libs/FProperty.h"
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* This linked-production test supplies deterministic X transport, never an
 * X server. Native-X peers separately qualify actual server behavior. */
#undef XEventsQueued
#undef XNextEvent
#undef XPutBackEvent

static manager_selection state;
static _XPrivDisplay display;
static uint64_t clock_ns;
static bool expire_read, error_during_write;
static unsigned creates, destroys, notices;
static Window created_root;
static XSelectionEvent replies[128];
static uint32_t pairs[MANAGER_SELECTION_PAIR_MAX * 2];
static size_t pair_bytes;
static XEvent queue[MANAGER_SELECTION_EVENT_MAX + 8];
static size_t queued;

typedef struct
{
	unsigned long serial;
	Window window;
	Atom property, type;
	int count;
	unsigned long values[MANAGER_SELECTION_PAIR_MAX * 2];
} recorded_write;

static recorded_write writes[128];
static size_t written;

uint64_t events_monotonic_ns(void)
{
	return clock_ns;
}

static unsigned long request(Display *connection)
{
	return ++((_XPrivDisplay)connection)->request;
}

Window XCreateWindow(Display *connection, Window parent, int x, int y,
	unsigned width, unsigned height, unsigned border, int depth,
	unsigned window_class, Visual *visual, unsigned long mask,
	XSetWindowAttributes *attributes)
{
	(void)x; (void)y; (void)visual;
	assert(width == 1 && height == 1 && border == 0 && depth == 0);
	assert(window_class == InputOnly && mask == CWEventMask);
	assert(attributes->event_mask == PropertyChangeMask);
	request(connection);
	created_root = parent;
	creates++;
	return 300;
}

int XDestroyWindow(Display *connection, Window window)
{
	assert(window == state.marker_window);
	request(connection);
	destroys++;
	return 1;
}

int XFlush(Display *connection)
{
	assert(connection == (Display *)display);
	return 1;
}

Status FSendEvent(Display *connection, Window window, Bool propagate,
	long mask, XEvent *event)
{
	assert(!propagate && mask == 0 && event->type == SelectionNotify);
	assert(window == event->xselection.requestor && notices < 128);
	request(connection);
	replies[notices++] = event->xselection;
	return 1;
}

int XChangeProperty(Display *connection, Window window, Atom property,
	Atom type, int format, int mode, const unsigned char *data, int count)
{
	assert(format == 32 && mode == PropModeReplace);
	assert(count >= 0 && count <= (int)(MANAGER_SELECTION_PAIR_MAX * 2));
	assert(written < 128);
	recorded_write *out = &writes[written++];

	out->serial = request(connection);
	out->window = window;
	out->property = property;
	out->type = type;
	out->count = count;
	memcpy(out->values, data, (size_t)count * sizeof(unsigned long));
	if (error_during_write && window != state.marker_window)
	{
		XErrorEvent error = {.display = connection, .serial = out->serial,
			.request_code = X_ChangeProperty, .error_code = BadAlloc};

		error_during_write = false;
		assert(manager_selection_error(&state, &error));
	}
	return 1;
}

FPropertyResult FPropertyRead(Display *connection, Window window, Atom property,
	Atom type, FPropertyLimits limits, unsigned char **data, size_t *bytes)
{
	(void)window; (void)property;
	assert(type == state.atom_pair && limits.format == 32);
	assert(limits.max_bytes == FVWM_SELECTION_PAIR_MAX_BYTES);
	request(connection);
	*data = NULL;
	*bytes = 0;
	if (expire_read)
		clock_ns += MANAGER_SELECTION_LIFETIME_NS;
	*data = malloc(pair_bytes + 1);
	assert(*data != NULL);
	memcpy(*data, pairs, pair_bytes);
	*bytes = pair_bytes;
	return FPROPERTY_OK;
}

int XEventsQueued(Display *connection, int mode)
{
	assert(connection == (Display *)display && mode == QueuedAlready);
	return (int)queued;
}

int XNextEvent(Display *connection, XEvent *event)
{
	assert(connection == (Display *)display && queued > 0);
	*event = queue[0];
	memmove(queue, queue + 1, --queued * sizeof(*queue));
	return 0;
}

int XPutBackEvent(Display *connection, XEvent *event)
{
	assert(connection == (Display *)display &&
		queued < sizeof(queue) / sizeof(*queue));
	memmove(queue + 1, queue, queued++ * sizeof(*queue));
	queue[0] = *event;
	return 0;
}

static void reset(void)
{
	memset(&state, 0, sizeof(state));
	memset(display, 0, sizeof(*display));
	clock_ns = 1000000;
	expire_read = error_during_write = false;
	creates = destroys = notices = 0;
	written = queued = pair_bytes = 0;
	const Atom targets[] = {50, 51, 52, 53};

	assert(manager_selection_init(&state, (Display *)display, 10, 20,
		30, 40, 41, targets, 100, clock_ns));
	assert(creates == 1 && created_root == 10);
}

static XSelectionRequestEvent conversion(Atom target, Atom property)
{
	XSelectionRequestEvent event = {.type = SelectionRequest,
		.display = (Display *)display, .owner = 20, .requestor = 90,
		.selection = 30, .target = target, .property = property, .time = 101};

	return event;
}

static void begin(Atom target)
{
	XSelectionRequestEvent event = conversion(target, 80);

	manager_selection_admit(&state, &event, clock_ns);
	manager_selection_service(&state);
}

static XEvent marker(void)
{
	XEvent event = {0};

	event.xproperty = (XPropertyEvent){.type = PropertyNotify,
		.display = (Display *)display, .window = state.marker_window,
		.atom = state.marker_atom, .serial = state.current.marker_serial,
		.state = PropertyNewValue};
	return event;
}

static XSelectionClearEvent loss(void)
{
	XSelectionClearEvent event = {.type = SelectionClear,
		.display = (Display *)display, .window = state.owner,
		.selection = state.selection, .time = 1};

	return event;
}

static bool error(unsigned long serial, unsigned opcode)
{
	XErrorEvent event = {.display = (Display *)display, .serial = serial,
		.request_code = opcode, .error_code = BadAlloc};

	return manager_selection_error(&state, &event);
}

static void acknowledge(void)
{
	XEvent event = marker();

	assert(manager_selection_event(&state, &event));
	assert(manager_selection_timeout_ms(&state, clock_ns) == 0);
	manager_selection_service(&state);
}

static void test_direct_errors_and_correlation(void)
{
	reset();
	begin(50);
	assert(written == 2 && notices == 0);
	assert(writes[0].type == XA_ATOM && writes[0].count == 4);
	assert(writes[0].values[0] == 50 && writes[0].values[3] == 53);
	assert(!error(writes[0].serial + 100, X_ChangeProperty));
	assert(!error(writes[0].serial, X_DeleteProperty));
	assert(error(writes[0].serial, X_ChangeProperty));
	assert(!state.current.marker_settled && notices == 0);
	XEvent event = marker();

	event.xproperty.send_event = True;
	assert(manager_selection_event(&state, &event));
	assert(!state.current.marker_settled);
	event = marker(); event.xproperty.serial++;
	assert(manager_selection_event(&state, &event));
	assert(!state.current.marker_settled);
	event = marker(); event.xproperty.state = PropertyDelete;
	assert(manager_selection_event(&state, &event));
	assert(!state.current.marker_settled);
	acknowledge();
	assert(notices == 1 && replies[0].property == None);
	assert(replies[0].requestor == 90 && replies[0].selection == 30 &&
		replies[0].target == 50 && replies[0].time == 101);
	begin(52); acknowledge();
	assert(notices == 2 && replies[1].property == 80);
	reset();
	error_during_write = true;
	begin(53); acknowledge();
	assert(notices == 1 && replies[0].property == None);
	/* An unset marker must not alias a valid data serial at native wrap. */
	reset(); display->request = ULONG_MAX;
	error_during_write = true;
	begin(53);
	assert(writes[0].serial == 0 && !state.current.marker_settled);
	acknowledge();
	assert(notices == 1 && replies[0].property == None);
}

static void test_multiple_two_barriers(void)
{
	reset();
	const uint32_t requested[] = {52, 81, 53, 82, 50, 81, 51, 83, 52, 80};

	memcpy(pairs, requested, sizeof(requested)); pair_bytes = sizeof(requested);
	begin(51);
	assert(written == 4 && notices == 0);
	assert(error(writes[1].serial, X_ChangeProperty));
	acknowledge();
	assert(state.current.control_phase && notices == 0 && written == 6);
	assert(writes[4].property == 80 && writes[4].type == 40);
	const unsigned long expected[] = {52, 81, 53, 0, 50, 81, 51, 0, 52, 0};

	assert(writes[4].count == 10 &&
		memcmp(writes[4].values, expected, sizeof(expected)) == 0);
	assert(!error(writes[0].serial, X_ChangeProperty));
	assert(error(writes[4].serial, X_ChangeProperty));
	assert(notices == 0);
	acknowledge();
	assert(notices == 1 && replies[0].property == None);
	reset();
	begin(51); acknowledge();
	assert(state.current.control_phase && notices == 0);
	acknowledge();
	assert(notices == 1 && replies[0].property == 80);
	reset(); expire_read = true;
	begin(51);
	assert(written == 0 && notices == 1 && replies[0].property == None);
}

static void test_deadlines_queue_and_late_errors(void)
{
	reset(); begin(50);
	unsigned long serial = writes[0].serial;
	XSelectionRequestEvent event = conversion(52, 80);

	for (size_t i = 0; i < MANAGER_SELECTION_QUEUE_MAX; i++)
		manager_selection_admit(&state, &event, clock_ns);
	assert(state.count == MANAGER_SELECTION_QUEUE_MAX);
	manager_selection_admit(&state, &event, clock_ns);
	assert(notices == 1 && replies[0].property == None);
	clock_ns += MANAGER_SELECTION_LIFETIME_NS;
	manager_selection_service(&state);
	assert(state.current.inflight && state.current.cancelled && state.count == 0);
	assert(notices == MANAGER_SELECTION_QUEUE_MAX + 2);
	assert(manager_selection_timeout_ms(&state, clock_ns) == -1);
	assert(error(serial, X_ChangeProperty));
	assert(state.current.inflight);
	acknowledge();
	assert(!state.current.inflight && notices == MANAGER_SELECTION_QUEUE_MAX + 2);
	begin(52); acknowledge();
	assert(replies[notices - 1].property == 80);
	reset(); begin(50);
	assert(error(state.creation_serial, X_CreateWindow));
	assert(state.current.inflight && !state.current.marker_settled);
	assert(error(writes[0].serial, X_ChangeProperty));
	assert(!state.current.marker_settled);
	assert(error(state.current.marker_serial, X_ChangeProperty));
	manager_selection_service(&state);
	assert(notices == 1 && replies[0].property == None);
	assert(!state.current.inflight && state.marker_unusable);
	reset(); begin(50);
	manager_selection_close(&state);
	assert(!state.active && destroys == 1);
	assert(error(writes[0].serial, X_ChangeProperty));
	assert(error(state.current.marker_serial, X_ChangeProperty));
	manager_selection_service(&state);
	assert(notices == 0);
	manager_selection_close(&state); assert(destroys == 1);
}

static void test_clear_identity_and_modal_order(void)
{
	reset();
	XSelectionClearEvent event = loss(), wrong = event;

	wrong.send_event = True; assert(!manager_selection_clear(&state, &wrong));
	wrong = event;
	wrong.selection++;
	assert(!manager_selection_clear(&state, &wrong));
	wrong = event;
	wrong.window++;
	assert(!manager_selection_clear(&state, &wrong));
	wrong = event;
	wrong.display = NULL;
	assert(!manager_selection_clear(&state, &wrong));
	wrong = event;
	wrong.type = SelectionNotify;
	assert(!manager_selection_clear(&state, &wrong));
	state.process++;
	assert(!manager_selection_clear(&state, &event));
	manager_selection_close(&state); assert(destroys == 0);
	state.process = getpid();
	assert(manager_selection_clear(&state, &event) && !state.active);
	assert(!manager_selection_clear(&state, &event));
	reset(); begin(50);
	queue[0].xkey = (XKeyEvent){.type = KeyPress, .keycode = 9};
	queue[1] = marker();
	queue[2].xkey = (XKeyEvent){.type = KeyRelease, .keycode = 9};
	queued = 3;
	manager_selection_interaction(&state);
	assert(notices == 1 && replies[0].property == 80 && queued == 2);
	assert(queue[0].type == KeyPress && queue[1].type == KeyRelease);
	reset(); begin(50);
	queue[0].xselectionclear = loss();
	queue[1] = marker(); queued = 2;
	manager_selection_interaction(&state);
	assert(state.loss_pending && state.active && notices == 0 && queued == 2);
	manager_selection_service(&state); assert(notices == 0);
	assert(manager_selection_clear(&state, &queue[0].xselectionclear));
	reset(); begin(50);
	XEvent extracted = marker();

	assert(manager_selection_defer_event(&state, &extracted));
	queue[0].xselectionclear = loss(); queued = 1;
	manager_selection_interaction(&state);
	assert(state.loss_pending && notices == 0 && queued == 1);
	reset(); begin(50); extracted = marker();
	assert(manager_selection_defer_event(&state, &extracted));
	for (size_t i = 0; i < MANAGER_SELECTION_EVENT_MAX + 1; i++)
		queue[i].xkey = (XKeyEvent){.type = KeyPress, .keycode = (unsigned)i};
	queued = MANAGER_SELECTION_EVENT_MAX + 1;
	manager_selection_interaction(&state);
	assert(notices == 0 && state.deferred_marker_valid);
	for (size_t i = 0; i < queued; i++) assert(queue[i].xkey.keycode == i);
	/* Return to ordinary dispatch, with no further modal pump. Draining
	 * one input exposes the complete prefix and must unblock the marker. */
	XEvent input;

	XNextEvent((Display *)display, &input);
	assert(input.type == KeyPress && input.xkey.keycode == 0);
	manager_selection_ordinary(&state);
	assert(notices == 1 && replies[0].property == 80);
	assert(queued == MANAGER_SELECTION_EVENT_MAX);
	for (size_t i = 0; i < queued; i++) assert(queue[i].xkey.keycode == i + 1);
	reset(); begin(50); extracted = marker();
	assert(manager_selection_defer_event(&state, &extracted));
	for (size_t i = 0; i < MANAGER_SELECTION_EVENT_MAX + 1; i++)
		queue[i].xkey = (XKeyEvent){.type = KeyPress, .keycode = (unsigned)i};
	queued = MANAGER_SELECTION_EVENT_MAX + 1;
	clock_ns += MANAGER_SELECTION_LIFETIME_NS;
	manager_selection_ordinary(&state);
	assert(notices == 1 && replies[0].property == None);
	assert(state.current.inflight && state.current.cancelled);
	queued = 0; /* Ordinary input dispatch has drained the retained records. */
	manager_selection_ordinary(&state);
	assert(!state.current.inflight && notices == 1);
	begin(52); acknowledge();
	assert(notices == 2 && replies[1].property == 80);
}

int main(void)
{
	display = calloc(1, sizeof(*display)); assert(display != NULL);
	test_direct_errors_and_correlation();
	test_multiple_two_barriers();
	test_deadlines_queue_and_late_errors();
	test_clear_identity_and_modal_order();
	free(display);
	return 0;
}
