/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "libs/clay_layout_eval.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <string.h>

static void output_contract(void)
{
	clay_layout_eval_box boxes[] = {
		{1, true, 0, 0, 10.4f, 24},
		{2, true, 10.4f, 0, 10.3f, 24},
		{3, true, 20.7f, 0, 10.3f, 24}
	};
	int widths[] = {-1, -1, -1};

	assert(clay_layout_eval_widths(31, 24, boxes, 3, widths));
	assert(widths[0] == 10 && widths[1] == 11 && widths[2] == 10);
	memset(widths, 0xff, sizeof(widths));
	boxes[1].id = boxes[0].id;
	assert(!clay_layout_eval_widths(31, 24, boxes, 3, widths));
	assert(widths[0] == -1 && widths[1] == -1 && widths[2] == -1);
	boxes[1].id = 2;
	boxes[1].found = false;
	assert(!clay_layout_eval_widths(31, 24, boxes, 3, widths));
	boxes[1].found = true;
	boxes[1].x = NAN;
	assert(!clay_layout_eval_widths(31, 24, boxes, 3, widths));
	boxes[1].x = 11.4f;
	assert(!clay_layout_eval_widths(31, 24, boxes, 3, widths));
	boxes[1].x = 9.4f;
	assert(!clay_layout_eval_widths(31, 24, boxes, 3, widths));
	boxes[1].x = 10.4f;
	boxes[1].height = 25;
	assert(!clay_layout_eval_widths(31, 24, boxes, 3, widths));
	boxes[1].height = 24;
	boxes[2].width = INFINITY;
	assert(!clay_layout_eval_widths(31, 24, boxes, 3, widths));
	boxes[2].width = 9.3f;
	assert(!clay_layout_eval_widths(31, 24, boxes, 3, widths));
	boxes[2].width = 11.3f;
	assert(!clay_layout_eval_widths(31, 24, boxes, 3, widths));
	boxes[2].width = 10.3f;
	boxes[0].id = 0;
	assert(!clay_layout_eval_widths(31, 24, boxes, 3, widths));
	assert(!clay_layout_eval_widths(31, 24, NULL, 3, widths));
	assert(!clay_layout_eval_widths(31, 24, boxes, 0, widths));
	assert(!clay_layout_eval_widths(31, 24, boxes, 257, widths));
	assert(!clay_layout_eval_widths(31, 24, boxes, 3, NULL));
	assert(widths[0] == -1 && widths[1] == -1 && widths[2] == -1);
}

static void input_contract(clay_layout_eval *state)
{
	int minimum[] = {20, 40};
	unsigned weights[] = {1, 1};
	int widths[] = {-1, -1};

	assert(clay_layout_eval_row(state, 120, 24, minimum, weights, 2,
		widths) == CLAY_LAYOUT_EVAL_READY);
	/* This intentionally differs from native minimum-plus-surplus [50,70]. */
	assert(widths[0] == 60 && widths[1] == 60);
	widths[0] = widths[1] = -1;
	weights[1] = 2;
	assert(clay_layout_eval_row(state, 120, 24, minimum, weights, 2,
		widths) == CLAY_LAYOUT_EVAL_REFUSED);
	weights[1] = 1;
	assert(clay_layout_eval_row(state, 59, 24, minimum, weights, 2,
		widths) == CLAY_LAYOUT_EVAL_REFUSED);
	minimum[0] = 0;
	assert(clay_layout_eval_row(state, 120, 24, minimum, weights, 2,
		widths) == CLAY_LAYOUT_EVAL_REFUSED);
	minimum[0] = 20;
	weights[0] = weights[1] = 0;
	assert(clay_layout_eval_row(state, 120, 24, minimum, weights, 2,
		widths) == CLAY_LAYOUT_EVAL_REFUSED);
	weights[0] = weights[1] = UINT_MAX;
	assert(clay_layout_eval_row(state, 120, 24, minimum, weights, 2,
		widths) == CLAY_LAYOUT_EVAL_REFUSED);
	weights[0] = weights[1] = 1;
	assert(clay_layout_eval_row(state, 120, 0, minimum, weights, 2,
		widths) == CLAY_LAYOUT_EVAL_REFUSED);
	assert(clay_layout_eval_row(state, INT_MAX, 24, minimum, weights, 2,
		widths) == CLAY_LAYOUT_EVAL_REFUSED);
	assert(clay_layout_eval_row(state, 120, 24, NULL, weights, 2,
		widths) == CLAY_LAYOUT_EVAL_REFUSED);
	assert(clay_layout_eval_row(state, 120, 24, minimum, NULL, 2,
		widths) == CLAY_LAYOUT_EVAL_REFUSED);
	assert(clay_layout_eval_row(state, 120, 24, minimum, weights, 0,
		widths) == CLAY_LAYOUT_EVAL_REFUSED);
	assert(clay_layout_eval_row(state, 120, 24, minimum, weights, 257,
		widths) == CLAY_LAYOUT_EVAL_REFUSED);
	assert(widths[0] == -1 && widths[1] == -1);
}

static void bounded_rows(clay_layout_eval *state)
{
	int minimum[CLAY_LAYOUT_EVAL_MAX_ITEMS];
	unsigned weights[CLAY_LAYOUT_EVAL_MAX_ITEMS];
	int widths[CLAY_LAYOUT_EVAL_MAX_ITEMS];
	void *arena = state->arena;
	size_t bytes = state->arena_bytes;

	for (size_t i = 0; i < CLAY_LAYOUT_EVAL_MAX_ITEMS; i++)
	{
		minimum[i] = 1;
		weights[i] = 1;
	}
	for (size_t count = 1; count <= CLAY_LAYOUT_EVAL_MAX_ITEMS; count++)
	{
		int sum = 0;

		assert(clay_layout_eval_row(state, 32767, 32767, minimum,
			weights, count, widths) == CLAY_LAYOUT_EVAL_READY);
		for (size_t i = 0; i < count; i++)
		{
			assert(widths[i] >= 1);
			sum += widths[i];
		}
		assert(sum == 32767);
		assert(state->arena == arena && state->arena_bytes == bytes);
	}
	assert(clay_layout_eval_row(state, 257, 1, minimum, weights, 256,
		widths) == CLAY_LAYOUT_EVAL_READY);
	assert(clay_layout_eval_row(state, 1, 1, minimum, weights, 1,
		widths) == CLAY_LAYOUT_EVAL_READY && widths[0] == 1);
}

int main(void)
{
	clay_layout_eval state = {0}, other = {0};

	output_contract();
	assert(!clay_layout_eval_init(NULL, CLAY_LAYOUT_EVAL_ARENA_LIMIT));
	assert(!clay_layout_eval_init(&state, 1));
	assert(!state.arena && !state.context && state.arena_bytes == 0);
	assert(clay_layout_eval_init(&state, CLAY_LAYOUT_EVAL_ARENA_LIMIT));
	assert(state.arena_bytes > 0 &&
		state.arena_bytes <= CLAY_LAYOUT_EVAL_ARENA_LIMIT);
	assert(!clay_layout_eval_init(&state, CLAY_LAYOUT_EVAL_ARENA_LIMIT));
	input_contract(&state);
	bounded_rows(&state);
	assert(clay_layout_eval_init(&other, CLAY_LAYOUT_EVAL_ARENA_LIMIT));
	assert(other.context != state.context && other.arena != state.arena);
	input_contract(&other);
	input_contract(&state);
	clay_layout_eval_destroy(&other);
	clay_layout_eval_destroy(&state);
	assert(!state.context && !state.arena && state.arena_bytes == 0);
	clay_layout_eval_destroy(&state);
	clay_layout_eval_destroy(NULL);
	assert(clay_layout_eval_init(&state, CLAY_LAYOUT_EVAL_ARENA_LIMIT));
	input_contract(&state);
	clay_layout_eval_destroy(&state);
	return 0;
}
