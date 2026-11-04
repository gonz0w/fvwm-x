/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "libs/graphics_eval_policy.h"

static void geometry(void)
{
	graphics_eval_rect out = {1, 1, 1, 1};
	graphics_eval_rect rect = {10, 20, 100, 80};
	graphics_eval_rect clip = {30, 15, 50, 100};

	assert(graphics_eval_prepare(rect, clip, 640, 480, &out) ==
		GRAPHICS_EVAL_READY);
	assert(out.x == 30 && out.y == 20 && out.width == 50 &&
		out.height == 80);
	rect = (graphics_eval_rect){-5, -5, 16, 16};
	clip = (graphics_eval_rect){-10, -10, 32, 32};
	assert(graphics_eval_prepare(rect, clip, 8, 8, &out) ==
		GRAPHICS_EVAL_READY);
	assert(out.x == 0 && out.y == 0 && out.width == 8 && out.height == 8);
	rect = (graphics_eval_rect){900, 900, 16, 16};
	assert(graphics_eval_prepare(rect, clip, 640, 480, &out) ==
		GRAPHICS_EVAL_EMPTY);
	assert(out.x == 0 && out.y == 0 && !out.width && !out.height);
	rect = (graphics_eval_rect){0, 0, 512, 512};
	clip = rect;
	assert(graphics_eval_prepare(rect, clip, 1024, 1024, &out) ==
		GRAPHICS_EVAL_READY);
	rect.height++;
	assert(graphics_eval_prepare(rect, clip, 1024, 1024, &out) ==
		GRAPHICS_EVAL_REFUSED);
	assert(!out.width && !out.height);
	rect = (graphics_eval_rect){0, 0, 2049, 1};
	assert(graphics_eval_prepare(rect, clip, 4096, 4096, &out) ==
		GRAPHICS_EVAL_REFUSED);
	rect = (graphics_eval_rect){INT_MAX, INT_MIN, UINT_MAX, UINT_MAX};
	assert(graphics_eval_prepare(rect, clip, 1024, 1024, &out) ==
		GRAPHICS_EVAL_REFUSED);
	rect = (graphics_eval_rect){0, 0, 0, 10};
	assert(graphics_eval_prepare(rect, clip, 1024, 1024, &out) ==
		GRAPHICS_EVAL_EMPTY);
	assert(graphics_eval_prepare(rect, clip, 0, 1024, &out) ==
		GRAPHICS_EVAL_REFUSED);
	assert(graphics_eval_prepare(rect, clip, 1024, 1024, NULL) ==
		GRAPHICS_EVAL_REFUSED);
	rect = (graphics_eval_rect){40000, 0, 1, 1};
	clip = rect;
	assert(graphics_eval_prepare(rect, clip, 65535, 1, &out) ==
		GRAPHICS_EVAL_REFUSED);
}

static void bounded_timeline(void)
{
	graphics_eval_transition t = {0};
	uint32_t rgb = UINT32_MAX;
	uint64_t token;

	assert(graphics_eval_transition_timeout(&t, 0) == -1);
	assert(graphics_eval_transition_begin(&t, 100, 7, 3, 0, 0xffffff,
		true, false) == GRAPHICS_EVAL_START_ACTIVE);
	token = t.token;
	assert(graphics_eval_transition_timeout(&t, 100) == 15);
	assert(graphics_eval_transition_sample(&t, token, 7, 3, 114, true,
		&rgb) == GRAPHICS_EVAL_TICK_WAIT);
	assert(t.paints == 0 && rgb == 0);
	for (unsigned int i = 1; i <= 8; i++)
	{
		enum graphics_eval_tick status =
			graphics_eval_transition_sample(
			&t, token, 7, 3, 100 + i * 15, true, &rgb);

		assert(status == (i == 8 ? GRAPHICS_EVAL_TICK_FINISH :
			GRAPHICS_EVAL_TICK_FRAME));
		assert(t.paints == i);
		if (i == 4)
			assert(rgb == 0x808080);
	}
	assert(rgb == 0xffffff && !t.active);
	assert(graphics_eval_transition_timeout(&t, 220) == -1);
	assert(graphics_eval_transition_sample(&t, token, 7, 3, 999, true,
		&rgb) == GRAPHICS_EVAL_TICK_IDLE);
	assert(t.paints == 8);

	assert(graphics_eval_transition_begin(&t, 1000, 7, 3, 0xff0000,
		0x0000ff, true, false) == GRAPHICS_EVAL_START_ACTIVE);
	token = t.token;
	assert(graphics_eval_transition_sample(&t, token, 7, 3, 1060, true,
		&rgb) == GRAPHICS_EVAL_TICK_FRAME);
	assert(rgb == 0x800080 && t.paints == 1);
	/* Skip missed frames; never drain them as a burst. */
	assert(graphics_eval_transition_sample(&t, token, 7, 3, 1060, true,
		&rgb) == GRAPHICS_EVAL_TICK_WAIT);
	assert(t.paints == 1);
	assert(graphics_eval_transition_sample(&t, token, 7, 3, 9000, true,
		&rgb) == GRAPHICS_EVAL_TICK_FINISH);
	assert(rgb == 0x0000ff && t.paints == 2);
}

static void replacement_and_lifetime(void)
{
	graphics_eval_transition t = {0};
	uint32_t rgb;
	uint64_t old_token, current;

	assert(graphics_eval_transition_begin(&t, 0, 1, 1, 0, 0xffffff,
		true, false) == GRAPHICS_EVAL_START_ACTIVE);
	old_token = t.token;
	assert(graphics_eval_transition_sample(&t, old_token, 1, 1, 60,
		true, &rgb) == GRAPHICS_EVAL_TICK_FRAME);
	assert(graphics_eval_transition_begin(&t, 60, 1, 1, 0xffffff, 0,
		true, false) == GRAPHICS_EVAL_START_ACTIVE);
	current = t.token;
	assert(current > old_token && t.from_rgb == 0x808080);
	assert(graphics_eval_transition_sample(&t, old_token, 1, 1, 9999,
		true, &rgb) == GRAPHICS_EVAL_TICK_STALE);
	assert(t.active && t.token == current && t.paints == 0);
	assert(graphics_eval_transition_sample(&t, current, 1, 2, 75,
		true, &rgb) == GRAPHICS_EVAL_TICK_CANCELLED);
	assert(!t.active && t.token > current);

	assert(graphics_eval_transition_begin(&t, 100, 2, 4, 0, 0xffffff,
		true, false) == GRAPHICS_EVAL_START_ACTIVE);
	current = t.token;
	assert(graphics_eval_transition_sample(&t, current, 2, 4, 120,
		false, &rgb) == GRAPHICS_EVAL_TICK_CANCELLED);
	assert(graphics_eval_transition_timeout(&t, 120) == -1);
	assert(graphics_eval_transition_begin(&t, 200, 2, 4, 0, 0xffffff,
		true, true) == GRAPHICS_EVAL_START_IMMEDIATE);
	assert(!t.active && t.last_rgb == 0xffffff && t.paints == 0);
	assert(graphics_eval_transition_begin(&t, 200, 2, 4, 0, 0xffffff,
		false, false) == GRAPHICS_EVAL_START_IMMEDIATE);
	assert(!t.active && graphics_eval_transition_timeout(&t, 200) == -1);
	assert(graphics_eval_transition_begin(&t, 200, 2, 4, 0x123456,
		0x123456, true, false) == GRAPHICS_EVAL_START_IMMEDIATE);
	assert(!t.active);

	assert(graphics_eval_transition_begin(&t, 200, 2, 4, 0, 0xffffff,
		true, false) == GRAPHICS_EVAL_START_ACTIVE);
	current = t.token;
	assert(graphics_eval_transition_begin(&t, 210, 2, 4, 0, 0xffffff,
		true, false) == GRAPHICS_EVAL_START_ACTIVE);
	assert(t.token == current && t.started_ms == 200);
	assert(graphics_eval_transition_sample(&t, current, 2, 4, 199,
		true, &rgb) == GRAPHICS_EVAL_TICK_CANCELLED);
}

static void admitted_clock_floor(void)
{
	graphics_eval_transition t = {0};
	uint32_t rgb;
	uint64_t token;

	assert(graphics_eval_transition_begin(&t, 100, 9, 1, 0, 0xffffff,
		true, false) == GRAPHICS_EVAL_START_ACTIVE);
	token = t.token;
	assert(graphics_eval_transition_sample(&t, token, 9, 1, 160, true,
		&rgb) == GRAPHICS_EVAL_TICK_FRAME);
	assert(graphics_eval_transition_sample(&t, token, 9, 1, 150, true,
		&rgb) == GRAPHICS_EVAL_TICK_CANCELLED);
	assert(!t.active);
	assert(graphics_eval_transition_begin(&t, 200, 9, 1, 0, 0xffffff,
		true, false) == GRAPHICS_EVAL_START_ACTIVE);
	token = t.token;
	assert(graphics_eval_transition_sample(&t, token, 9, 1, 210, true,
		&rgb) == GRAPHICS_EVAL_TICK_WAIT);
	assert(graphics_eval_transition_sample(&t, token, 9, 1, 205, true,
		&rgb) == GRAPHICS_EVAL_TICK_CANCELLED);
	assert(graphics_eval_transition_begin(&t, 300, 9, 1, 0, 0xffffff,
		true, false) == GRAPHICS_EVAL_START_ACTIVE);
	assert(graphics_eval_transition_begin(&t, 310, 9, 1, 0, 0xffffff,
		true, false) == GRAPHICS_EVAL_START_ACTIVE);
	assert(graphics_eval_transition_begin(&t, 305, 9, 1, 0, 0xffffff,
		true, false) == GRAPHICS_EVAL_START_REFUSED);
	assert(!t.active);
}

static void boundaries(void)
{
	graphics_eval_transition t = {0};
	uint32_t rgb = UINT32_MAX;

	assert(graphics_eval_transition_begin(&t, UINT64_MAX, 1, 0, 0, 1,
		true, false) == GRAPHICS_EVAL_START_REFUSED);
	assert(!t.active);
	assert(graphics_eval_transition_begin(&t, UINT64_MAX - 120, 1, 0,
		0, 0xffffff, true, false) == GRAPHICS_EVAL_START_ACTIVE);
	assert(graphics_eval_transition_sample(&t, t.token, 1, 0,
		UINT64_MAX, true, &rgb) == GRAPHICS_EVAL_TICK_FINISH);
	assert(rgb == 0xffffff && !t.active);
	assert(graphics_eval_transition_begin(&t, 0, 1, 0, 0x1000000, 0,
		true, false) == GRAPHICS_EVAL_START_REFUSED);
	t.token = UINT64_MAX;
	assert(graphics_eval_transition_begin(&t, 0, 1, 0, 0, 1,
		true, false) == GRAPHICS_EVAL_START_REFUSED);
	assert(!t.active && t.token == UINT64_MAX);
	graphics_eval_transition_cancel(&t);
	assert(t.token == UINT64_MAX);
	assert(graphics_eval_transition_sample(NULL, 0, 0, 0, 0, false,
		&rgb) == GRAPHICS_EVAL_TICK_REFUSED);
	assert(rgb == 0);
	assert(graphics_eval_transition_begin(NULL, 0, 1, 0, 0, 1,
		true, false) == GRAPHICS_EVAL_START_REFUSED);
}

int main(void)
{
	geometry();
	bounded_timeline();
	replacement_and_lifetime();
	admitted_clock_floor();
	boundaries();
	return 0;
}
