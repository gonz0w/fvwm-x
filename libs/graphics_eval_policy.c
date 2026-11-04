/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include "graphics_eval_policy.h"
#include <limits.h>
#include <stddef.h>

static int64_t maximum(int64_t a, int64_t b)
{
	return a > b ? a : b;
}

static int64_t minimum(int64_t a, int64_t b)
{
	return a < b ? a : b;
}

enum graphics_eval_preparation graphics_eval_prepare(
	graphics_eval_rect operation, graphics_eval_rect clip,
	uint32_t window_width, uint32_t window_height, graphics_eval_rect *out)
{
	int64_t left, top, right, bottom;

	if (!out)
		return GRAPHICS_EVAL_REFUSED;
	*out = (graphics_eval_rect){0};
	if (!window_width || !window_height || window_width > UINT16_MAX ||
	    window_height > UINT16_MAX || clip.width > UINT16_MAX ||
	    clip.height > UINT16_MAX)
		return GRAPHICS_EVAL_REFUSED;
	if (!operation.width || !operation.height ||
	    !clip.width || !clip.height)
		return GRAPHICS_EVAL_EMPTY;
	if (operation.width > GRAPHICS_EVAL_MAX_SIDE ||
	    operation.height > GRAPHICS_EVAL_MAX_SIDE ||
	    (uint64_t)operation.width * operation.height >
	    GRAPHICS_EVAL_MAX_PIXELS)
		return GRAPHICS_EVAL_REFUSED;
	left = maximum(0, maximum(operation.x, clip.x));
	top = maximum(0, maximum(operation.y, clip.y));
	right = minimum(window_width, minimum(
		(int64_t)operation.x + operation.width,
		(int64_t)clip.x + clip.width));
	bottom = minimum(window_height, minimum(
		(int64_t)operation.y + operation.height,
		(int64_t)clip.y + clip.height));
	if (left >= right || top >= bottom)
		return GRAPHICS_EVAL_EMPTY;
	if (left > INT16_MAX || top > INT16_MAX)
		return GRAPHICS_EVAL_REFUSED;
	*out = (graphics_eval_rect){
		(int32_t)left, (int32_t)top,
		(uint32_t)(right - left), (uint32_t)(bottom - top)};
	return GRAPHICS_EVAL_READY;
}

void graphics_eval_transition_cancel(graphics_eval_transition *state)
{
	if (!state)
		return;
	state->active = false;
	state->last_rgb = state->to_rgb;
	if (state->token != UINT64_MAX)
		state->token++;
}

enum graphics_eval_start graphics_eval_transition_begin(
	graphics_eval_transition *state, uint64_t now_ms, uint64_t identity,
	uint64_t generation, uint32_t from_rgb, uint32_t to_rgb,
	bool visible, bool reduced_motion)
{
	bool continuous, invalid;
	uint32_t start_rgb;

	if (!state)
		return GRAPHICS_EVAL_START_REFUSED;
	continuous = state->active && state->identity == identity &&
		state->generation == generation;
	invalid = !identity || from_rgb > 0xffffff || to_rgb > 0xffffff ||
		(now_ms < state->observed_ms);
	if (!invalid && continuous && visible && !reduced_motion &&
	    state->to_rgb == to_rgb)
	{
		state->observed_ms = now_ms;
		return GRAPHICS_EVAL_START_ACTIVE;
	}
	start_rgb = continuous ? state->last_rgb : from_rgb;
	if (state->token == UINT64_MAX)
	{
		state->active = false;
		return GRAPHICS_EVAL_START_REFUSED;
	}
	state->token++;
	state->active = false;
	if (invalid || (visible && !reduced_motion && start_rgb != to_rgb &&
	    now_ms > UINT64_MAX - GRAPHICS_EVAL_DURATION_MS))
		return GRAPHICS_EVAL_START_REFUSED;
	state->identity = identity;
	state->generation = generation;
	state->from_rgb = start_rgb;
	state->to_rgb = to_rgb;
	state->last_rgb = to_rgb;
	state->paints = 0;
	state->started_ms = state->observed_ms = now_ms;
	state->next_ms = state->deadline_ms = 0;
	if (!visible || reduced_motion || start_rgb == to_rgb)
		return GRAPHICS_EVAL_START_IMMEDIATE;
	state->last_rgb = start_rgb;
	state->deadline_ms = now_ms + GRAPHICS_EVAL_DURATION_MS;
	state->next_ms = now_ms + GRAPHICS_EVAL_FRAME_MS;
	state->active = true;
	return GRAPHICS_EVAL_START_ACTIVE;
}

static uint32_t interpolate(uint32_t from, uint32_t to, uint64_t elapsed)
{
	uint32_t rgb = 0;

	for (unsigned int shift = 0; shift <= 16; shift += 8)
	{
		uint64_t a = (from >> shift) & 255u;
		uint64_t b = (to >> shift) & 255u;
		uint64_t channel = (a * (GRAPHICS_EVAL_DURATION_MS - elapsed) +
			b * elapsed + GRAPHICS_EVAL_DURATION_MS / 2) /
			GRAPHICS_EVAL_DURATION_MS;

		rgb |= (uint32_t)channel << shift;
	}
	return rgb;
}

enum graphics_eval_tick graphics_eval_transition_sample(
	graphics_eval_transition *state, uint64_t token, uint64_t identity,
	uint64_t generation, uint64_t now_ms, bool visible, uint32_t *rgb)
{
	if (!rgb)
		return GRAPHICS_EVAL_TICK_REFUSED;
	*rgb = 0;
	if (!state)
		return GRAPHICS_EVAL_TICK_REFUSED;
	*rgb = state->last_rgb;
	if (!state->active)
		return GRAPHICS_EVAL_TICK_IDLE;
	if (token != state->token)
		return GRAPHICS_EVAL_TICK_STALE;
	if (now_ms < state->observed_ms)
	{
		graphics_eval_transition_cancel(state);
		*rgb = state->last_rgb;
		return GRAPHICS_EVAL_TICK_CANCELLED;
	}
	state->observed_ms = now_ms;
	if (identity != state->identity || generation != state->generation ||
	    !visible)
	{
		graphics_eval_transition_cancel(state);
		*rgb = state->last_rgb;
		return GRAPHICS_EVAL_TICK_CANCELLED;
	}
	if (state->paints >= GRAPHICS_EVAL_MAX_PAINTS)
	{
		graphics_eval_transition_cancel(state);
		*rgb = state->last_rgb;
		return GRAPHICS_EVAL_TICK_REFUSED;
	}
	if (now_ms < state->next_ms)
		return GRAPHICS_EVAL_TICK_WAIT;
	state->paints++;
	if (now_ms >= state->deadline_ms ||
	    state->paints == GRAPHICS_EVAL_MAX_PAINTS)
	{
		state->active = false;
		state->last_rgb = *rgb = state->to_rgb;
		return GRAPHICS_EVAL_TICK_FINISH;
	}
	state->last_rgb = *rgb = interpolate(state->from_rgb, state->to_rgb,
		now_ms - state->started_ms);
	state->next_ms = state->deadline_ms - now_ms <= GRAPHICS_EVAL_FRAME_MS ?
		state->deadline_ms : now_ms + GRAPHICS_EVAL_FRAME_MS;
	return GRAPHICS_EVAL_TICK_FRAME;
}

int graphics_eval_transition_timeout(
	const graphics_eval_transition *state, uint64_t now_ms)
{
	uint64_t remaining;

	if (!state || !state->active)
		return -1;
	if (now_ms < state->observed_ms || now_ms >= state->next_ms)
		return 0;
	remaining = state->next_ms - now_ms;
	return (int)(remaining > GRAPHICS_EVAL_DURATION_MS ?
		GRAPHICS_EVAL_DURATION_MS : remaining);
}
