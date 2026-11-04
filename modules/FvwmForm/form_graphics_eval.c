/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libs/PictureBase.h"
#include "FvwmForm.h"
#include "form_graphics_eval.h"

/* Private comparison state contains values only. Re-resolve a child XID in a
 * bounded current list; never keep an Item/DrawTable/font/picture pointer. */
static struct
{
	graphics_eval_transition transition;
	graphics_eval_stats stats;
	Window frame, child;
	int size, pos_x, pos_y, on;
	uint64_t revision;
	Bool mapped, servicing;
	enum graphics_observer_operation operation;
} evaluation;

static graphics_eval_observer observer;
static Bool observer_initialized;

static graphics_eval_observation observation(
	enum graphics_observer_event event,
	enum graphics_observer_reason reason,
	const graphics_eval_transition *before)
{
	graphics_eval_transition *after = &evaluation.transition;
	graphics_eval_observation value = {0};

	value.clock_valid = graphics_eval_clock(&value.ms);
	if (reason == GRAPHICS_REASON_CLOCK)
		observer.clock_failed = true;
	value.event = event;
	value.reason = reason;
	value.token = before->token;
	value.next_token = after->token;
	value.identity = after->identity;
	value.revision = after->generation;
	value.active_before = before->active;
	value.active_after = after->active;
	value.service_paints = after->paints;
	value.from_rgb = after->from_rgb;
	value.to_rgb = after->to_rgb;
	value.last_rgb = after->last_rgb;
	value.timeout = value.clock_valid ?
		graphics_eval_transition_timeout(after, value.ms) : 0;
	value.operation = evaluation.operation;
	value.service = evaluation.servicing;
	return value;
}

static void observer_exit(void)
{
	graphics_eval_transition before;
	graphics_eval_observation value;

	if (!observer.enabled)
		return;
	/* No X or widget access, including after native cleanup. */
	before = evaluation.transition;
	graphics_eval_transition_cancel(&evaluation.transition);
	value = observation(GRAPHICS_OBSERVER_TERMINAL,
		GRAPHICS_REASON_EXIT, &before);
#define COPY_STAT(name) \
	observer.stats.name = evaluation.stats.name; \
	if (evaluation.stats.name == UINT64_MAX) observer.saturated = true;
	GRAPHICS_OBSERVER_STATS(COPY_STAT)
#undef COPY_STAT
	graphics_eval_observer_finish(&observer, value);
	graphics_eval_observer_export(&observer, STDERR_FILENO, 0,
		graphics_eval_cairo_backend());
}

void form_eval_observer_init(void)
{
	const char *trace;

	if (observer_initialized)
		return;
	observer_initialized = True;
	trace = getenv("FVWM_GRAPHICS_EVAL_TRACE");
	if (!trace || strcmp(trace, "1") != 0)
		return;
	graphics_eval_observer_init(&observer, true);
	/* Registration failure leaves no valid terminal evidence. */
	if (atexit(observer_exit) != 0)
		observer.enabled = false;
}

graphics_eval_draw_scope form_eval_draw_start(
	enum graphics_observer_operation operation)
{
	graphics_eval_draw_scope scope = {0};

	form_eval_observer_init();
	if (!observer.enabled)
		return scope;
	scope.enabled = true;
	scope.operation = evaluation.operation = operation;
	scope.request = NextRequest(dpy);
	scope.paint_issued = observer.counters[GRAPHICS_OBSERVER_PAINT_ISSUED];
	scope.fallbacks = observer.counters[GRAPHICS_OBSERVER_NATIVE_FALLBACKS];
	return scope;
}

void form_eval_draw_end(graphics_eval_draw_scope scope, Bool fallback)
{
	graphics_eval_observation value;

	if (!scope.enabled)
		return;
	if (fallback)
		graphics_eval_observer_count(&observer,
			GRAPHICS_OBSERVER_NATIVE_FALLBACKS, 1);
	value = observation(GRAPHICS_OBSERVER_DRAW, GRAPHICS_REASON_NONE,
		&evaluation.transition);
	value.operation = scope.operation;
	/* Unsigned Xlib serial subtraction also handles its native wrap. */
	value.issued = (unsigned long)(NextRequest(dpy) - scope.request);
	value.paint_issued =
		observer.counters[GRAPHICS_OBSERVER_PAINT_ISSUED] -
		scope.paint_issued;
	if (observer.counters[GRAPHICS_OBSERVER_NATIVE_FALLBACKS] !=
	    scope.fallbacks)
		value.outcome = GRAPHICS_OUTCOME_NATIVE;
	graphics_eval_observer_count(&observer, GRAPHICS_OBSERVER_ISSUED,
		value.issued);
	graphics_eval_observer_count(&observer, evaluation.servicing ?
		GRAPHICS_OBSERVER_SERVICE_DRAWS :
		GRAPHICS_OBSERVER_SYNCHRONOUS_DRAWS,
		1);
	graphics_eval_observer_note(&observer, value);
	evaluation.operation = GRAPHICS_OPERATION_NONE;
}

static Bool observed_paint(Drawable destination, uint32_t width,
	uint32_t height, graphics_eval_rect bounds, graphics_eval_rect clip,
	const graphics_eval_operation *operations, size_t length)
{
	unsigned long request = 0;
	uint64_t committed = 0, empty = 0;
	Bool result;

	if (observer.enabled)
	{
		request = NextRequest(dpy);
		committed = evaluation.stats.committed;
		empty = evaluation.stats.empty;
	}
	result = graphics_eval_paint(dpy, destination, Pvisual, Pdepth,
		width, height, bounds, clip, operations, length, True,
		getenv("FVWM_GRAPHICS_EVAL_REFUSE") != NULL, &evaluation.stats);
	if (observer.enabled)
	{
		graphics_eval_observation value = observation(
			GRAPHICS_OBSERVER_PAINT, GRAPHICS_REASON_NONE,
			&evaluation.transition);

		value.paint_issued =
			(unsigned long)(NextRequest(dpy) - request);
		value.outcome = !result ? GRAPHICS_OUTCOME_REFUSED :
			evaluation.stats.committed != committed ?
			GRAPHICS_OUTCOME_COMMITTED :
			evaluation.stats.empty != empty ?
			GRAPHICS_OUTCOME_EMPTY : 0;
		graphics_eval_observer_count(&observer,
			GRAPHICS_OBSERVER_PAINT_ISSUED, value.paint_issued);
		graphics_eval_observer_note(&observer, value);
	}
	return result;
}

static Item *choice(void)
{
	unsigned int count = 0;

	if (!evaluation.mapped || evaluation.frame != CF.frame)
		return NULL;
	for (Item *item = root_item_ptr; item && count < 256;
	     item = item->header.next, count++)
		if ((Window)item->header.win == evaluation.child &&
		    item->type == I_CHOICE && item->header.size_y ==
		    evaluation.size &&
		    item->header.pos_x == evaluation.pos_x &&
		    item->header.pos_y == evaluation.pos_y &&
		    item->choice.on == evaluation.on)
			return item;
	return NULL;
}

static void cancel_reason(Bool repaint,
	enum graphics_observer_reason reason)
{
	graphics_eval_transition before = {0};

	form_eval_observer_init();
	if (observer.enabled)
		before = evaluation.transition;
	Item *item = repaint ? choice() : NULL;

	graphics_eval_transition_cancel(&evaluation.transition);
	if (evaluation.revision != UINT64_MAX)
		evaluation.revision++;
	evaluation.child = None;
	if (observer.enabled)
		graphics_eval_observer_note(&observer, observation(
			GRAPHICS_OBSERVER_CANCEL, reason, &before));
	if (item)
	{
		XClearArea(dpy, item->header.win, 0, 0, 0, 0, False);
		RedrawItem(item, 0, NULL);
	}
}

void form_eval_cancel(Bool repaint)
{
	cancel_reason(repaint, GRAPHICS_REASON_EXTERNAL);
}

void form_eval_event(const XEvent *event)
{
	Window target = event->xany.window;

	switch (event->type)
	{
	case MapNotify: target = event->xmap.window; break;
	case UnmapNotify: target = event->xunmap.window; break;
	case DestroyNotify: target = event->xdestroywindow.window; break;
	case ConfigureNotify: target = event->xconfigure.window; break;
	}
	if (target == CF.frame)
	{
		if (event->type == MapNotify)
			evaluation.mapped = True;
		if (event->type == UnmapNotify || event->type == DestroyNotify)
			evaluation.mapped = False;
		if (event->type == UnmapNotify || event->type == DestroyNotify)
			cancel_reason(False, event->type == UnmapNotify ?
				GRAPHICS_REASON_UNMAP :
				GRAPHICS_REASON_DESTROY);
		else if (event->type == ConfigureNotify)
			cancel_reason(event->xconfigure.width ==
				CF.max_width &&
				event->xconfigure.height == CF.total_height,
				GRAPHICS_REASON_CONFIGURE);
	}
	else if (target == evaluation.child)
	{
		if (event->type == UnmapNotify || event->type == DestroyNotify)
			cancel_reason(False, event->type == UnmapNotify ?
				GRAPHICS_REASON_UNMAP :
				GRAPHICS_REASON_DESTROY);
		else if (event->type == ConfigureNotify)
			cancel_reason(event->xconfigure.width ==
				evaluation.size &&
				event->xconfigure.height == evaluation.size,
				GRAPHICS_REASON_CONFIGURE);
	}
}

void form_eval_choice_begin(Item *item)
{
	uint64_t now;
	graphics_eval_transition before = {0};
	const char *motion = getenv("FVWM_GRAPHICS_EVAL_MOTION");

	cancel_reason(True, GRAPHICS_REASON_REPLACE);
	if (!item || item->type != I_CHOICE || item->header.size_y < 4 ||
	    item->header.size_y > (int)GRAPHICS_EVAL_MAX_SIDE ||
	    !graphics_eval_supported(dpy, Pvisual, Pdepth) ||
	    !graphics_eval_clock(&now) || evaluation.revision == UINT64_MAX)
	{
		if (observer.enabled)
			graphics_eval_observer_note(&observer, observation(
				GRAPHICS_OBSERVER_BEGIN,
				GRAPHICS_REASON_REFUSED,
				&evaluation.transition));
		return;
	}
	evaluation.frame = CF.frame;
	evaluation.child = (Window)item->header.win;
	evaluation.size = item->header.size_y;
	evaluation.pos_x = item->header.pos_x;
	evaluation.pos_y = item->header.pos_y;
	evaluation.on = item->choice.on;
	unsigned long hi = item->header.dt_ptr->dt_colors[c_itemhi];
	unsigned long lo = item->header.dt_ptr->dt_colors[c_itemlo];

	/* Only the bevel changes over time. The selected mark is immediate and
	 * remains under the ordinary draw owner. No desktop motion policy is
	 * invented: the private harness must explicitly request full motion. */
	if (observer.enabled)
		before = evaluation.transition;
	enum graphics_eval_start result = graphics_eval_transition_begin(
		&evaluation.transition, now,
		evaluation.child, evaluation.revision,
		(uint32_t)(item->choice.on ? hi : lo),
		(uint32_t)(item->choice.on ? lo : hi), choice() != NULL,
		!motion || strcmp(motion, "full") != 0);
	if (observer.enabled)
	{
		enum graphics_observer_reason reason =
			result == GRAPHICS_EVAL_START_REFUSED ?
			GRAPHICS_REASON_REFUSED :
			!motion || strcmp(motion, "full") != 0 ?
			GRAPHICS_REASON_REDUCED :
			evaluation.transition.from_rgb ==
			evaluation.transition.to_rgb ?
			GRAPHICS_REASON_EQUAL : !evaluation.transition.active ?
			GRAPHICS_REASON_INVISIBLE : GRAPHICS_REASON_NONE;

		graphics_eval_observer_note(&observer, observation(
			GRAPHICS_OBSERVER_BEGIN, reason, &before));
	}
}

void form_eval_service(void)
{
	graphics_eval_transition *state = &evaluation.transition;
	graphics_eval_transition before = {0};
	uint64_t now;
	uint32_t rgb;

	form_eval_observer_init();
	if (!state->active)
	{
		if (observer.enabled)
			graphics_eval_observer_count(&observer,
				GRAPHICS_OBSERVER_IDLE, 1);
		return;
	}
	Item *item = choice();

	if (!item || !graphics_eval_clock(&now))
	{
		cancel_reason(True, !item ? GRAPHICS_REASON_OWNER :
			GRAPHICS_REASON_CLOCK);
		return;
	}
	if (observer.enabled)
		before = *state;
	enum graphics_eval_tick tick = graphics_eval_transition_sample(state,
		state->token, evaluation.child, evaluation.revision,
		now, True, &rgb);

	if (tick == GRAPHICS_EVAL_TICK_FRAME ||
	    tick == GRAPHICS_EVAL_TICK_FINISH)
	{
		if (observer.enabled)
		{
			evaluation.servicing = True;
			graphics_eval_observer_count(&observer,
				GRAPHICS_OBSERVER_SERVICE_PAINTS, 1);
		}
		/* The existing draw owner regenerates the frame background. */
		XClearArea(dpy, item->header.win, 0, 0, 0, 0, False);
		RedrawItem(item, 0, NULL);
		if (observer.enabled)
		{
			graphics_eval_observer_note(&observer, observation(
				tick == GRAPHICS_EVAL_TICK_FRAME ?
				GRAPHICS_OBSERVER_FRAME :
				GRAPHICS_OBSERVER_FINISH,
				GRAPHICS_REASON_NONE, &before));
			evaluation.servicing = False;
		}
	}
	else if (tick == GRAPHICS_EVAL_TICK_WAIT)
	{
		if (observer.enabled)
			graphics_eval_observer_count(&observer,
				GRAPHICS_OBSERVER_WAIT, 1);
	}
	else if (tick == GRAPHICS_EVAL_TICK_CANCELLED ||
	    tick == GRAPHICS_EVAL_TICK_REFUSED)
	{
		if (observer.enabled)
			graphics_eval_observer_note(&observer, observation(
				GRAPHICS_OBSERVER_CANCEL, GRAPHICS_REASON_STATE,
				&before));
		form_eval_cancel(True);
	}
}

int form_eval_timeout(void)
{
	uint64_t now;

	if (observer.enabled)
		graphics_eval_observer_count(&observer,
			GRAPHICS_OBSERVER_TIMEOUT_QUERIES, 1);
	if (!evaluation.transition.active)
		return -1;
	if (!graphics_eval_clock(&now))
	{
		if (observer.enabled)
			observer.clock_failed = true;
		/* The next pre-FPending service handles failed-clock work. */
		return 0;
	}
	return graphics_eval_transition_timeout(&evaluation.transition, now);
}

Bool form_eval_rectangle(Item *item, int x, int y, int width, int height,
	unsigned long foreground, const XRectangle *clip)
{
	if (width < 1 || height < 1 || width >= INT32_MAX ||
	    height >= INT32_MAX ||
	    item->header.size_x < 1 || item->header.size_y < 1 ||
	    foreground > 0xffffff)
		return False;
	graphics_eval_rect bounds = {x, y, (uint32_t)width + 1,
		(uint32_t)height + 1};
	graphics_eval_rect clipping = clip ?
		(graphics_eval_rect){clip->x, clip->y, clip->width,
			clip->height} :
		(graphics_eval_rect){0, 0, (uint32_t)item->header.size_x,
		(uint32_t)item->header.size_y};
	graphics_eval_operation operation = {.kind = GRAPHICS_EVAL_RECTANGLE,
		.rectangle = bounds, .foreground = (uint32_t)foreground};

	return observed_paint(item->header.win,
		(uint32_t)item->header.size_x, (uint32_t)item->header.size_y,
		bounds, clipping, &operation, 1);
}

Bool form_eval_choice_outline(Item *item)
{
	int size = item->header.size_y;
	unsigned long hi = item->header.dt_ptr->dt_colors[c_itemhi];
	unsigned long lo = item->header.dt_ptr->dt_colors[c_itemlo];
	uint32_t first = (uint32_t)(item->choice.on ? lo : hi);
	uint32_t second = (uint32_t)(item->choice.on ? hi : lo);
	graphics_eval_rect bounds = {0, 0, (uint32_t)size, (uint32_t)size};

	if (size < 4 || size > (int)GRAPHICS_EVAL_MAX_SIDE || hi > 0xffffff ||
		lo > 0xffffff ||
	    !graphics_eval_supported(dpy, Pvisual, Pdepth))
		return False;
	if (evaluation.transition.active && choice() == item)
	{
		first = evaluation.transition.last_rgb;
		second = 0;
		for (unsigned int shift = 0; shift < 24; shift += 8)
			second |= (((hi >> shift) & 255u) + ((lo >> shift) &
				255u) -
				((first >> shift) & 255u)) << shift;
	}
	int d = size - 1;
	XSegment segments[8] = {
		{0, 0, 0, d}, {0, 0, d, 0}, {1, 1, 1, d - 1},
		{1, 1, d - 1, 1}, {1, d, d, d}, {2, d - 1, d, d - 1},
		{d, 1, d, d}, {d - 1, 2, d - 1, d}};
	graphics_eval_operation operations[8];

	for (unsigned int i = 0; i < 8; i++)
	{
		XSegment s = segments[i];
		operations[i] = (graphics_eval_operation){
			.kind = GRAPHICS_EVAL_LINE,
			.rectangle = {s.x1, s.y1,
				(uint32_t)(s.x2 - s.x1 + 1),
				(uint32_t)(s.y2 - s.y1 + 1)},
			.foreground = i < 4 ? first : second};
	}
	if (observed_paint(item->header.win,
		(uint32_t)size, (uint32_t)size, bounds, bounds, operations, 8))
		return True;
	/* The native evaluation twin shares transition policy, but uses the
	 * existing two segment batches and the borrowed, ordinary draw GC. */
	if (observer.enabled)
		graphics_eval_observer_count(&observer,
			GRAPHICS_OBSERVER_NATIVE_FALLBACKS, 1);
	XSetForeground(dpy, item->header.dt_ptr->dt_item_GC, first);
	XDrawSegments(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		segments, 4);
	XSetForeground(dpy, item->header.dt_ptr->dt_item_GC, second);
	XDrawSegments(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		segments + 4, 4);
	return True;
}
