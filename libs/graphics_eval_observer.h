/* Private value-only drawing comparison evidence; not a module protocol. */
#ifndef FVWM_GRAPHICS_EVAL_OBSERVER_H
#define FVWM_GRAPHICS_EVAL_OBSERVER_H

#include <stddef.h>
#include "graphics_eval_policy.h"

#define GRAPHICS_OBSERVER_CAPACITY 256u
#define GRAPHICS_OBSERVER_STATS(X) \
	X(attempts) X(refused) X(empty) X(committed) \
	X(pixmap_requests) X(pixmap_retires) X(gc_requests) X(gc_retires) \
	X(surface_requests) X(surface_retires) \
	X(context_requests) X(context_retires) \
	X(pattern_requests) X(pattern_retires) \
	X(preserve_copies) X(commit_copies) X(pixels_copied) \
	X(scratch_pixels_requested) X(peak_scratch_pixels)

enum graphics_observer_event
{
	GRAPHICS_OBSERVER_BEGIN, GRAPHICS_OBSERVER_CANCEL,
	GRAPHICS_OBSERVER_FRAME, GRAPHICS_OBSERVER_FINISH,
	GRAPHICS_OBSERVER_DRAW, GRAPHICS_OBSERVER_PAINT,
	GRAPHICS_OBSERVER_OVERFLOW, GRAPHICS_OBSERVER_TERMINAL
};

enum graphics_observer_reason
{
	GRAPHICS_REASON_NONE, GRAPHICS_REASON_REPLACE,
	GRAPHICS_REASON_EXTERNAL, GRAPHICS_REASON_UNMAP,
	GRAPHICS_REASON_DESTROY, GRAPHICS_REASON_CONFIGURE,
	GRAPHICS_REASON_FOCUS, GRAPHICS_REASON_OWNER,
	GRAPHICS_REASON_CLOCK, GRAPHICS_REASON_STATE,
	GRAPHICS_REASON_REDUCED, GRAPHICS_REASON_EQUAL,
	GRAPHICS_REASON_INVISIBLE, GRAPHICS_REASON_REFUSED,
	GRAPHICS_REASON_EXIT
};

enum graphics_observer_operation
{
	GRAPHICS_OPERATION_NONE, GRAPHICS_OPERATION_FORM_LIST,
	GRAPHICS_OPERATION_FORM_CHOICE, GRAPHICS_OPERATION_BUTTONS_FOCUS,
	GRAPHICS_OPERATION_BUTTONS_INDICATOR
};

enum graphics_observer_counter
{
	GRAPHICS_OBSERVER_IDLE, GRAPHICS_OBSERVER_WAIT,
	GRAPHICS_OBSERVER_TIMEOUT_QUERIES, GRAPHICS_OBSERVER_SERVICE_PAINTS,
	GRAPHICS_OBSERVER_SYNCHRONOUS_DRAWS, GRAPHICS_OBSERVER_SERVICE_DRAWS,
	GRAPHICS_OBSERVER_ISSUED, GRAPHICS_OBSERVER_PAINT_ISSUED,
	GRAPHICS_OBSERVER_NATIVE_FALLBACKS, GRAPHICS_OBSERVER_COUNTER_COUNT
};

enum graphics_observer_outcome
{
	GRAPHICS_OUTCOME_COMMITTED = 1, GRAPHICS_OUTCOME_EMPTY = 2,
	GRAPHICS_OUTCOME_REFUSED = 4, GRAPHICS_OUTCOME_NATIVE = 8
};

typedef struct
{
	uint64_t sequence, ms, token, next_token, identity, revision;
	uint64_t service_paints, issued, paint_issued;
	uint32_t from_rgb, to_rgb, last_rgb;
	int timeout;
	enum graphics_observer_event event;
	enum graphics_observer_reason reason;
	enum graphics_observer_operation operation;
	unsigned int outcome;
	bool clock_valid, active_before, active_after, service;
} graphics_eval_observation;

typedef struct
{
	graphics_eval_observation records[GRAPHICS_OBSERVER_CAPACITY];
	uint64_t counters[GRAPHICS_OBSERVER_COUNTER_COUNT];
#define GRAPHICS_OBSERVER_FIELD(name) uint64_t name;
	struct { GRAPHICS_OBSERVER_STATS(GRAPHICS_OBSERVER_FIELD) } stats;
#undef GRAPHICS_OBSERVER_FIELD
	uint64_t sequence, dropped;
	size_t length;
	bool enabled, closed, overflow, saturated, clock_failed, exported;
} graphics_eval_observer;

/* Caller-owned values for a complete selected operation, including fallback.
 * Request numbers are copied by adapters only while observation is enabled. */
typedef struct
{
	uint64_t request, paint_issued, fallbacks;
	enum graphics_observer_operation operation;
	bool enabled;
} graphics_eval_draw_scope;

void graphics_eval_observer_init(graphics_eval_observer *owner, bool enabled);
bool graphics_eval_observer_note(graphics_eval_observer *owner,
	graphics_eval_observation value);
void graphics_eval_observer_count(graphics_eval_observer *owner,
	enum graphics_observer_counter counter, uint64_t amount);
/* Two reserved records preserve explicit overflow and terminal facts. */
bool graphics_eval_observer_finish(graphics_eval_observer *owner,
	graphics_eval_observation terminal);
bool graphics_eval_observer_complete(const graphics_eval_observer *owner);
/* One bounded write per JSON line; no retry or X/widget access. module is 0
 * for Form, 1 for Buttons. Only these fixed labels can reach the log. */
bool graphics_eval_observer_export(graphics_eval_observer *owner,
	int descriptor, unsigned int module, bool cairo_backend);

#endif
