/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include <stdlib.h>
#include <string.h>
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
	Bool mapped;
} evaluation;

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

void form_eval_cancel(Bool repaint)
{
	Item *item = repaint ? choice() : NULL;

	graphics_eval_transition_cancel(&evaluation.transition);
	if (evaluation.revision != UINT64_MAX)
		evaluation.revision++;
	evaluation.child = None;
	if (item)
	{
		XClearArea(dpy, item->header.win, 0, 0, 0, 0, False);
		RedrawItem(item, 0, NULL);
	}
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
			form_eval_cancel(False);
		else if (event->type == ConfigureNotify)
			form_eval_cancel(event->xconfigure.width ==
				CF.max_width &&
				event->xconfigure.height == CF.total_height);
	}
	else if (target == evaluation.child)
	{
		if (event->type == UnmapNotify || event->type == DestroyNotify)
			form_eval_cancel(False);
		else if (event->type == ConfigureNotify)
			form_eval_cancel(event->xconfigure.width ==
				evaluation.size &&
				event->xconfigure.height == evaluation.size);
	}
}

void form_eval_choice_begin(Item *item)
{
	uint64_t now;
	const char *motion = getenv("FVWM_GRAPHICS_EVAL_MOTION");

	form_eval_cancel(True);
	if (!item || item->type != I_CHOICE || item->header.size_y < 4 ||
	    item->header.size_y > (int)GRAPHICS_EVAL_MAX_SIDE ||
	    !graphics_eval_supported(dpy, Pvisual, Pdepth) ||
	    !graphics_eval_clock(&now) || evaluation.revision == UINT64_MAX)
		return;
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
	graphics_eval_transition_begin(&evaluation.transition, now,
		evaluation.child, evaluation.revision,
		(uint32_t)(item->choice.on ? hi : lo),
		(uint32_t)(item->choice.on ? lo : hi), choice() != NULL,
		!motion || strcmp(motion, "full") != 0);
}

void form_eval_service(void)
{
	graphics_eval_transition *state = &evaluation.transition;
	uint64_t now;
	uint32_t rgb;

	if (!state->active)
		return;
	Item *item = choice();

	if (!item || !graphics_eval_clock(&now))
	{
		form_eval_cancel(True);
		return;
	}
	enum graphics_eval_tick tick = graphics_eval_transition_sample(state,
		state->token, evaluation.child, evaluation.revision, now,
			True, &rgb);

	if (tick == GRAPHICS_EVAL_TICK_FRAME || tick ==
		GRAPHICS_EVAL_TICK_FINISH)
	{
		/* RedrawItem alone does not clear a selected choice.
		 * Regenerate its background before every frame so antialiasing
		 * cannot accumulate. */
		XClearArea(dpy, item->header.win, 0, 0, 0, 0, False);
		RedrawItem(item, 0, NULL);
	}
	else if (tick == GRAPHICS_EVAL_TICK_CANCELLED ||
	    tick == GRAPHICS_EVAL_TICK_REFUSED)
		form_eval_cancel(True);
}

int form_eval_timeout(void)
{
	uint64_t now;

	if (!evaluation.transition.active)
		return -1;
	if (!graphics_eval_clock(&now))
		return 0; /* next pre-FPending service retires failed-clock
			work */
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

	return graphics_eval_paint(dpy, item->header.win, Pvisual, Pdepth,
		(uint32_t)item->header.size_x, (uint32_t)item->header.size_y,
		bounds, clipping, &operation, 1, True,
		getenv("FVWM_GRAPHICS_EVAL_REFUSE") != NULL, &evaluation.stats);
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
	if (graphics_eval_paint(dpy, item->header.win, Pvisual, Pdepth,
		(uint32_t)size, (uint32_t)size, bounds, bounds, operations, 8,
		True, getenv("FVWM_GRAPHICS_EVAL_REFUSE") != NULL,
			&evaluation.stats))
		return True;
	/* The native evaluation twin shares transition policy, but uses the
	 * existing two segment batches and the borrowed, ordinary draw GC. */
	XSetForeground(dpy, item->header.dt_ptr->dt_item_GC, first);
	XDrawSegments(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		segments, 4);
	XSetForeground(dpy, item->header.dt_ptr->dt_item_GC, second);
	XDrawSegments(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		segments + 4, 4);
	return True;
}
