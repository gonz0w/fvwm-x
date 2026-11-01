#ifndef FVWMSCRIPT_SCRIPT_SELECTION_H
#define FVWMSCRIPT_SCRIPT_SELECTION_H

#include "script_message.h"
#include "script_selection_policy.h"
#include <X11/Xlib.h>

typedef struct {
	Window window;
	Atom selection;
	Time time;
	uint64_t acquired_ns;
	uint64_t epoch;
	char *text;                 /* Immutable malloc-owned PRIMARY only. */
	size_t length;              /* Wire bytes, excluding local NUL. */
} script_selection_owner;

typedef struct {
	Display *display;
	script_message_queue *queue;
	Atom targets, timestamp, multiple, atom_pair, clock_property;
	uint64_t epoch;
	script_selection_owner primary, messages;
} script_selection_state;

/* Module-local owner side only. No reader adoption or selection-wait pump.
 * CurrentTime is resolved with an owned PropertyNotify/XSync round trip;
 * individual Xlib calls still require a responsive server. */
bool script_selection_init(script_selection_state *state, Display *display,
	script_message_queue *queue);
bool script_selection_primary_set(script_selection_state *state,
	const char *text, size_t length, Time time);
bool script_selection_private_set(script_selection_state *state,
	Atom selection);
/* Borrowed until ownership changes; checks actual ownership before self-paste.
 * Returns NULL on loss/absence and initializes length. Never reads Scrapt. */
const char *script_selection_primary_text(script_selection_state *state,
	size_t *length);
bool script_selection_private_owned(script_selection_state *state);
/* Clear only a matching current epoch/window/selection after actual loss.
 * True identifies a retired PRIMARY so the caller can unselect its UI. */
bool script_selection_clear(script_selection_state *state,
	const XSelectionClearEvent *event);
void script_selection_request(script_selection_state *state,
	const XSelectionRequestEvent *request);
/* Destroy only owned windows; never assign a selection to root or None.
 * False skips X calls on connection loss, retaining the same memory cleanup. */
void script_selection_release(script_selection_state *state, bool release_x);

#endif
