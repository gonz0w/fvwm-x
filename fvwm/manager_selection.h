#ifndef FVWM_MANAGER_SELECTION_H
#define FVWM_MANAGER_SELECTION_H

#include "libs/fvwm_x11.h"
#include "property_policy.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define MANAGER_SELECTION_QUEUE_MAX 16U
#define MANAGER_SELECTION_EVENT_MAX 32U
#define MANAGER_SELECTION_PAIR_MAX (FVWM_SELECTION_PAIR_MAX_BYTES / 8U)
#define MANAGER_SELECTION_LIFETIME_NS UINT64_C(1000000000)

typedef struct
{
	XSelectionRequestEvent event;
	uint64_t admitted_ns;
} manager_selection_request;

typedef struct
{
	unsigned long serial;
	size_t pair;
	bool failed;
} manager_selection_write;

typedef struct
{
	bool inflight;
	bool multiple;
	bool control_phase;
	bool cancelled;
	bool notified;
	bool marker_submitted;
	bool marker_settled;
	bool marker_failed;
	unsigned long marker_serial;
	manager_selection_request request;
	unsigned long pairs[MANAGER_SELECTION_PAIR_MAX * 2];
	size_t pair_count;
	manager_selection_write writes[MANAGER_SELECTION_PAIR_MAX];
	size_t write_count;
} manager_selection_conversion;

/* Core-manager metadata only. Initialize a zeroed record once. Keep it through
 * XCloseDisplay, including after close/loss: late errors still own serials.
 * All entry points run on the core thread. No managed-window pointers or
 * callbacks are retained. targets are TARGETS/MULTIPLE/TIMESTAMP/VERSION. */
typedef struct
{
	Display *display;
	pid_t process;
	Window owner;
	Window marker_window;
	Atom selection;
	Atom atom_pair;
	Atom marker_atom;
	Atom targets[4];
	Time acquired;
	uint64_t acquired_ns;
	bool active;
	bool closing;
	bool loss_pending;
	bool deferred_marker_valid;
	XEvent deferred_marker;
	bool creation_pending;
	bool marker_unusable;
	unsigned long creation_serial;
	manager_selection_request queue[MANAGER_SELECTION_QUEUE_MAX];
	size_t head;
	size_t count;
	manager_selection_conversion current;
} manager_selection;

bool manager_selection_init(manager_selection *state, Display *display,
	Window root, Window owner, Atom selection, Atom atom_pair,
	Atom marker_atom, const Atom targets[4], Time acquired,
	uint64_t acquired_ns);
void manager_selection_admit(manager_selection *state,
	const XSelectionRequestEvent *event, uint64_t now_ns);
bool manager_selection_clear(manager_selection *state,
	const XSelectionClearEvent *event);
bool manager_selection_event(manager_selection *state, const XEvent *event);
bool manager_selection_error(manager_selection *state,
	const XErrorEvent *event);
void manager_selection_service(manager_selection *state);
/* Modal masks may skip PropertyNotify. These functions preserve input/cache
 * order, defer markers selected past other events, and leave loss dispatch
 * with the ordinary lifecycle handler. A partial scan cannot release a
 * deferred marker. */
bool manager_selection_defer_event(manager_selection *state,
	const XEvent *event);
bool manager_selection_defer_clear(manager_selection *state,
	const XSelectionClearEvent *event);
void manager_selection_interaction(manager_selection *state);
void manager_selection_ordinary(manager_selection *state);
int manager_selection_timeout_ms(const manager_selection *state,
	uint64_t now_ns);
void manager_selection_close(manager_selection *state);

#endif
