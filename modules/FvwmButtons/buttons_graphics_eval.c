/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include <stdlib.h>
#include <string.h>
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
	Bool mapped;
} evaluation;

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

void buttons_eval_cancel(Bool repaint)
{
	button_info *button = repaint ? focused() : NULL;

	graphics_eval_transition_cancel(&evaluation.transition);
	if (evaluation.revision != UINT64_MAX)
		evaluation.revision++;
	evaluation.window = None;
	if (button)
		RedrawButton(button, DRAW_FORCE, NULL);
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
			buttons_eval_cancel(False);
		else if (event->type == ConfigureNotify)
			buttons_eval_cancel(event->xconfigure.width == Width &&
				event->xconfigure.height == Height);
	}
	if (event->type == FocusOut || event->type == EnterNotify ||
	    event->type == LeaveNotify)
		buttons_eval_cancel(True);
}

void buttons_eval_focus_begin(button_info *button, unsigned long from)
{
	uint64_t now;
	const char *motion = getenv("FVWM_GRAPHICS_EVAL_MOTION");

	buttons_eval_cancel(False);
	if (!eligible(button) ||
	    !graphics_eval_supported(Dpy, Pvisual, Pdepth) ||
	    !graphics_eval_clock(&now) || from > 0xffffff ||
	    evaluation.revision == UINT64_MAX)
		return;
	unsigned long to = buttons_eval_foreground(button);

	if (to > 0xffffff)
		return;
	evaluation.window = MyWindow;
	evaluation.index = button->n;
	evaluation.x = buttonXPos(button, button->n);
	evaluation.y = buttonYPos(button, button->n);
	evaluation.width = buttonWidth(button);
	evaluation.height = buttonHeight(button);
	graphics_eval_transition_begin(&evaluation.transition, now,
		(uint64_t)button->n + 1, evaluation.revision,
		(uint32_t)from, (uint32_t)to, focused() != NULL,
		!motion || strcmp(motion, "full") != 0);
}

void buttons_eval_service(void)
{
	graphics_eval_transition *state = &evaluation.transition;
	uint64_t now;
	uint32_t rgb;

	if (!state->active)
		return;
	button_info *button = focused();

	if (!button || !graphics_eval_clock(&now))
	{
		buttons_eval_cancel(True);
		return;
	}
	enum graphics_eval_tick tick = graphics_eval_transition_sample(state,
		state->token, (uint64_t)evaluation.index + 1,
			evaluation.revision,
		now, True, &rgb);

	if (tick == GRAPHICS_EVAL_TICK_FRAME || tick ==
		GRAPHICS_EVAL_TICK_FINISH)
		RedrawButton(button, DRAW_FORCE, NULL);
	else if (tick == GRAPHICS_EVAL_TICK_CANCELLED ||
	    tick == GRAPHICS_EVAL_TICK_REFUSED)
		buttons_eval_cancel(True);
}

int buttons_eval_timeout(void)
{
	uint64_t now;

	if (!evaluation.transition.active)
		return -1;
	if (!graphics_eval_clock(&now))
		return 0; /* next pre-FPending service retires failed-clock
			work */
	return graphics_eval_transition_timeout(&evaluation.transition, now);
}

static Bool paint(graphics_eval_rect bounds,
	const graphics_eval_operation *operations, size_t length)
{
	if (Width < 1 || Height < 1)
		return False;
	graphics_eval_rect clip = {0, 0, (uint32_t)Width, (uint32_t)Height};

	return graphics_eval_paint(Dpy, MyWindow, Pvisual, Pdepth,
		clip.width, clip.height, bounds, clip, operations, length, True,
		getenv("FVWM_GRAPHICS_EVAL_REFUSE") != NULL, &evaluation.stats);
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
			buttons_eval_cancel(False);
		else
			foreground = evaluation.transition.last_rgb;
	}
	graphics_eval_rect bounds = {x, y, (uint32_t)width + 1,
		(uint32_t)height + 1};
	graphics_eval_operation operation = {.kind = GRAPHICS_EVAL_RECTANGLE,
		.rectangle = bounds, .foreground = (uint32_t)foreground};

	if (paint(bounds, &operation, 1))
		return True;
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
