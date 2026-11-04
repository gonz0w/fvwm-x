/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "FvwmButtons.h"
#include "button.h"
#include "draw.h"
#include "keyboard.h"
#include "libs/Colorset.h"
#include "libs/PictureBase.h"
#include "buttons_graphics_eval.h"

/* Existing FvwmButtons owner caches actual client dimensions here. Container
 * width/height exclude frame/padding and cannot bound these window draws. */
extern int Width, Height;

static struct
{
	graphics_eval_transition transition;
	graphics_eval_stats stats;
	Window window;
	int index, x, y, width, height;
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
	graphics_eval_observer_export(&observer, STDERR_FILENO, 1,
		graphics_eval_cairo_backend());
}

static void buttons_eval_observer_init(void)
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

graphics_eval_draw_scope buttons_eval_draw_start(
	enum graphics_observer_operation operation)
{
	graphics_eval_draw_scope scope = {0};

	buttons_eval_observer_init();
	if (!observer.enabled)
		return scope;
	scope.enabled = true;
	scope.operation = evaluation.operation = operation;
	scope.request = NextRequest(Dpy);
	scope.paint_issued = observer.counters[GRAPHICS_OBSERVER_PAINT_ISSUED];
	scope.fallbacks = observer.counters[GRAPHICS_OBSERVER_NATIVE_FALLBACKS];
	return scope;
}

void buttons_eval_draw_end(graphics_eval_draw_scope scope, Bool fallback)
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
	value.issued = (unsigned long)(NextRequest(Dpy) - scope.request);
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
		request = NextRequest(Dpy);
		committed = evaluation.stats.committed;
		empty = evaluation.stats.empty;
	}
	result = graphics_eval_paint(Dpy, destination, Pvisual, Pdepth,
		width, height, bounds, clip, operations, length, True,
		getenv("FVWM_GRAPHICS_EVAL_REFUSE") != NULL, &evaluation.stats);
	if (observer.enabled)
	{
		graphics_eval_observation value = observation(
			GRAPHICS_OBSERVER_PAINT, GRAPHICS_REASON_NONE,
			&evaluation.transition);

		value.paint_issued =
			(unsigned long)(NextRequest(Dpy) - request);
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

static Bool eligible(const button_info *button)
{
	return button && button->parent == UberButton && UberButton->c &&
		UberButton->c->num_buttons <= 256 && button->n >= 0 &&
		button->n < UberButton->c->num_buttons &&
		UberButton->c->buttons[button->n] == button &&
		!button->flags.b_Container && !button->flags.b_Swallow &&
		!button->flags.b_Panel && !button->flags.b_Tray &&
		!button->flags.b_Disabled && button->layout_prepared;
}

static button_info *focused(void)
{
	if (!evaluation.mapped || evaluation.window != MyWindow ||
	    !UberButton || !UberButton->c || evaluation.index < 0 ||
	    evaluation.index >= UberButton->c->num_buttons ||
	    UberButton->c->num_buttons > 256)
		return NULL;
	button_info *button = UberButton->c->buttons[evaluation.index];

	if (!eligible(button) || !buttons_is_focused(button) ||
	    buttonXPos(button, button->n) != evaluation.x ||
	    buttonYPos(button, button->n) != evaluation.y ||
	    buttonWidth(button) != evaluation.width ||
	    buttonHeight(button) != evaluation.height)
		return NULL;
	return button;
}

unsigned long buttons_eval_foreground(button_info *button)
{
	if (!button)
		return 0;
	int colorset = buttonColorset(button);

	return colorset < 0 ? buttonFore(button) : GetColorset(colorset)->fg;
}

static void cancel_reason(Bool repaint,
	enum graphics_observer_reason reason)
{
	graphics_eval_transition before = {0};

	buttons_eval_observer_init();
	if (observer.enabled)
		before = evaluation.transition;
	button_info *button = repaint ? focused() : NULL;

	graphics_eval_transition_cancel(&evaluation.transition);
	if (evaluation.revision != UINT64_MAX)
		evaluation.revision++;
	evaluation.window = None;
	if (observer.enabled)
		graphics_eval_observer_note(&observer, observation(
			GRAPHICS_OBSERVER_CANCEL, reason, &before));
	if (button)
		RedrawButton(button, DRAW_FORCE, NULL);
}

void buttons_eval_cancel(Bool repaint)
{
	cancel_reason(repaint, GRAPHICS_REASON_EXTERNAL);
}

void buttons_eval_event(const XEvent *event)
{
	Window target = event->xany.window;

	switch (event->type)
	{
	case MapNotify: target = event->xmap.window; break;
	case UnmapNotify: target = event->xunmap.window; break;
	case DestroyNotify: target = event->xdestroywindow.window; break;
	case ConfigureNotify: target = event->xconfigure.window; break;
	}
	if (target == MyWindow)
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
			cancel_reason(event->xconfigure.width == Width &&
				event->xconfigure.height == Height,
				GRAPHICS_REASON_CONFIGURE);
	}
	if (event->type == FocusOut || event->type == EnterNotify ||
	    event->type == LeaveNotify)
		cancel_reason(True, GRAPHICS_REASON_FOCUS);
}

void buttons_eval_focus_begin(button_info *button, unsigned long from)
{
	uint64_t now;
	graphics_eval_transition before = {0};
	const char *motion = getenv("FVWM_GRAPHICS_EVAL_MOTION");

	cancel_reason(False, GRAPHICS_REASON_REPLACE);
	if (!eligible(button) ||
	    !graphics_eval_supported(Dpy, Pvisual, Pdepth) ||
	    !graphics_eval_clock(&now) || from > 0xffffff ||
	    evaluation.revision == UINT64_MAX)
	{
		if (observer.enabled)
			graphics_eval_observer_note(&observer, observation(
				GRAPHICS_OBSERVER_BEGIN,
				GRAPHICS_REASON_REFUSED,
				&evaluation.transition));
		return;
	}
	unsigned long to = buttons_eval_foreground(button);

	if (to > 0xffffff)
	{
		if (observer.enabled)
			graphics_eval_observer_note(&observer, observation(
				GRAPHICS_OBSERVER_BEGIN,
				GRAPHICS_REASON_REFUSED,
				&evaluation.transition));
		return;
	}
	evaluation.window = MyWindow;
	evaluation.index = button->n;
	evaluation.x = buttonXPos(button, button->n);
	evaluation.y = buttonYPos(button, button->n);
	evaluation.width = buttonWidth(button);
	evaluation.height = buttonHeight(button);
	if (observer.enabled)
		before = evaluation.transition;
	enum graphics_eval_start result = graphics_eval_transition_begin(
		&evaluation.transition, now,
		(uint64_t)button->n + 1, evaluation.revision,
		(uint32_t)from, (uint32_t)to, focused() != NULL,
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

void buttons_eval_service(void)
{
	graphics_eval_transition *state = &evaluation.transition;
	graphics_eval_transition before = {0};
	uint64_t now;
	uint32_t rgb;

	buttons_eval_observer_init();
	if (!state->active)
	{
		if (observer.enabled)
			graphics_eval_observer_count(&observer,
				GRAPHICS_OBSERVER_IDLE, 1);
		return;
	}
	button_info *button = focused();

	if (!button || !graphics_eval_clock(&now))
	{
		cancel_reason(True, !button ? GRAPHICS_REASON_OWNER :
			GRAPHICS_REASON_CLOCK);
		return;
	}
	if (observer.enabled)
		before = *state;
	enum graphics_eval_tick tick = graphics_eval_transition_sample(state,
		state->token, (uint64_t)evaluation.index + 1,
		evaluation.revision,
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
		RedrawButton(button, DRAW_FORCE, NULL);
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
		buttons_eval_cancel(True);
	}
}

int buttons_eval_timeout(void)
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

static Bool paint(graphics_eval_rect bounds,
	const graphics_eval_operation *operations, size_t length)
{
	if (Width < 1 || Height < 1)
		return False;
	graphics_eval_rect clip = {0, 0, (uint32_t)Width, (uint32_t)Height};

	return observed_paint(MyWindow, clip.width, clip.height, bounds,
		clip, operations, length);
}

Bool buttons_eval_focus(button_info *button, int x, int y, int width,
	int height, unsigned long foreground)
{
	if (width < 1 || height < 1 || width >= INT32_MAX ||
	    height >= INT32_MAX ||
	    foreground > 0xffffff ||
	    !graphics_eval_supported(Dpy, Pvisual, Pdepth))
		return False;
	if (evaluation.transition.active && focused() == button)
	{
		/* A press/hover/state palette may supersede the focus role
		 * between ticks. This draw already owns clearing and paints
		 * its current role. */
		if (foreground != evaluation.transition.to_rgb ||
		    buttons_is_key_pressed(button) || CurrentButton == button ||
		    button->flags.b_Selected || button->flags.b_Hangon)
			cancel_reason(False, GRAPHICS_REASON_STATE);
		else
			foreground = evaluation.transition.last_rgb;
	}
	graphics_eval_rect bounds = {x, y, (uint32_t)width + 1,
		(uint32_t)height + 1};
	graphics_eval_operation operation = {.kind = GRAPHICS_EVAL_RECTANGLE,
		.rectangle = bounds, .foreground = (uint32_t)foreground};

	if (paint(bounds, &operation, 1))
		return True;
	if (observer.enabled)
		graphics_eval_observer_count(&observer,
			GRAPHICS_OBSERVER_NATIVE_FALLBACKS, 1);
	XSetForeground(Dpy, NormalGC, foreground);
	XDrawRectangle(Dpy, MyWindow, NormalGC, x, y, width, height);
	return True;
}

Bool buttons_eval_indicator(int x, int y, unsigned long foreground)
{
	graphics_eval_rect bounds = {x, y, 6, 6};
	graphics_eval_operation operations[2] = {
		{.kind = GRAPHICS_EVAL_LINE, .rectangle = bounds,
		 .foreground = (uint32_t)foreground},
		{.kind = GRAPHICS_EVAL_LINE, .rectangle = bounds,
		 .foreground = (uint32_t)foreground,
		 .flags = GRAPHICS_EVAL_REVERSE_LINE}};

	if (foreground > 0xffffff)
		return False;
	return paint(bounds, operations, 2);
}
