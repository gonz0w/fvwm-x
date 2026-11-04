/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "clay_layout_eval.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CLAY_DISABLE_SIMD
#define CLAY_IMPLEMENTATION
#include "tests/third_party/clay/clay.h"

#define CLAY_LAYOUT_EVAL_ELEMENTS 512
#define CLAY_LAYOUT_EVAL_WORDS 512
#define CLAY_LAYOUT_EVAL_EPSILON 0.125f

static void clay_layout_error(Clay_ErrorData error)
{
	clay_layout_eval *state = error.userData;

	if (state)
		state->failed = true;
}

bool clay_layout_eval_init(clay_layout_eval *state, size_t arena_limit)
{
	Clay_Context *previous;
	Clay_Arena arena;
	uint32_t required;

	if (!state || state->context || state->arena || !arena_limit)
		return false;
	state->arena_bytes = 0;
	state->failed = false;
	previous = Clay_GetCurrentContext();
	/* Capacity setters otherwise modify a different live context. This private
	 * owner restores the previous context at every API boundary. */
	Clay_SetCurrentContext(NULL);
	Clay_SetMaxElementCount(CLAY_LAYOUT_EVAL_ELEMENTS);
	Clay_SetMaxMeasureTextCacheWordCount(CLAY_LAYOUT_EVAL_WORDS);
	required = Clay_MinMemorySize();
	if (!required || required > CLAY_LAYOUT_EVAL_ARENA_LIMIT ||
		required > arena_limit)
	{
		Clay_SetCurrentContext(previous);
		return false;
	}
	/* Project fatal allocators would remove the explicit native fallback.
	 * Only this capped, private experimental arena uses fallible allocation. */
	state->arena = malloc(required);
	if (!state->arena)
	{
		Clay_SetCurrentContext(previous);
		return false;
	}
	state->arena_bytes = required;
	arena = Clay_CreateArenaWithCapacityAndMemory(required, state->arena);
	state->context = Clay_Initialize(arena, (Clay_Dimensions){1, 1},
		(Clay_ErrorHandler){clay_layout_error, state});
	Clay_SetCurrentContext(previous);
	if (!state->context || state->failed)
	{
		clay_layout_eval_destroy(state);
		return false;
	}
	return true;
}

void clay_layout_eval_destroy(clay_layout_eval *state)
{
	if (!state)
		return;
	if (state->context && Clay_GetCurrentContext() == state->context)
		Clay_SetCurrentContext(NULL);
	free(state->arena);
	memset(state, 0, sizeof(*state));
}

bool clay_layout_eval_widths(int available, int height,
	const clay_layout_eval_box *boxes, size_t count, int *widths)
{
	int prepared[CLAY_LAYOUT_EVAL_MAX_ITEMS];
	int previous_pixel = 0;
	float previous = 0;

	if (!boxes || !widths || !count || count > CLAY_LAYOUT_EVAL_MAX_ITEMS ||
		available < (int)count || available > CLAY_LAYOUT_EVAL_MAX_EXTENT ||
		height < 1 || height > CLAY_LAYOUT_EVAL_MAX_EXTENT)
		return false;
	for (size_t i = 0; i < count; i++)
	{
		const clay_layout_eval_box *box = &boxes[i];
		float right;
		int pixel;

		if (!box->found || !box->id || !isfinite(box->x) ||
			!isfinite(box->y) || !isfinite(box->width) ||
			!isfinite(box->height) || box->width <= 0 ||
			fabsf(box->x - previous) > CLAY_LAYOUT_EVAL_EPSILON ||
			fabsf(box->y) > CLAY_LAYOUT_EVAL_EPSILON ||
			fabsf(box->height - height) > CLAY_LAYOUT_EVAL_EPSILON)
			return false;
		for (size_t j = 0; j < i; j++)
			if (boxes[j].id == box->id)
				return false;
		right = box->x + box->width;
		if (!isfinite(right) || right <= 0 || right <= previous ||
			(double)right >= (double)available + 0.5)
			return false;
		/* Every shared boundary uses the same pixel rule. Float accumulation
		 * may drift within its final pixel; do not clamp an invalid endpoint. */
		pixel = (int)((double)right + 0.5);
		if (pixel <= previous_pixel || pixel > available ||
			(i + 1 == count && pixel != available))
			return false;
		prepared[i] = pixel - previous_pixel;
		previous_pixel = pixel;
		previous = right;
	}
	memcpy(widths, prepared, count * sizeof(*widths));
	return true;
}

enum clay_layout_eval_result clay_layout_eval_row(clay_layout_eval *state,
	int available, int height, const int *minimum, const unsigned *weights,
	size_t count, int *widths)
{
	clay_layout_eval_box boxes[CLAY_LAYOUT_EVAL_MAX_ITEMS];
	int copied[CLAY_LAYOUT_EVAL_MAX_ITEMS];
	int prepared[CLAY_LAYOUT_EVAL_MAX_ITEMS];
	uint32_t total = 0;
	Clay_Context *previous;
	bool valid;

	if (!state || !minimum || !weights || !widths || !count ||
		count > CLAY_LAYOUT_EVAL_MAX_ITEMS || available < (int)count ||
		available > CLAY_LAYOUT_EVAL_MAX_EXTENT || height < 1 ||
		height > CLAY_LAYOUT_EVAL_MAX_EXTENT)
		return CLAY_LAYOUT_EVAL_REFUSED;
	for (size_t i = 0; i < count; i++)
	{
		if (minimum[i] < 1 || minimum[i] > CLAY_LAYOUT_EVAL_MAX_EXTENT ||
			!weights[i] || weights[i] > CLAY_LAYOUT_EVAL_MAX_EXTENT ||
			weights[i] != weights[0])
			return CLAY_LAYOUT_EVAL_REFUSED;
		copied[i] = minimum[i];
		total += minimum[i];
	}
	if (total > (uint32_t)available)
		return CLAY_LAYOUT_EVAL_REFUSED;
	if (!state->context || !state->arena || state->failed)
		return CLAY_LAYOUT_EVAL_ERROR;
	previous = Clay_GetCurrentContext();
	Clay_SetCurrentContext(state->context);
	Clay_SetLayoutDimensions((Clay_Dimensions){available, height});
	Clay_BeginLayout();
	CLAY(CLAY_ID("fvwm_eval_row"), {
		.layout = {
			.sizing = {
				.width = CLAY_SIZING_FIXED(available),
				.height = CLAY_SIZING_FIXED(height)
			},
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		}
	})
	{
		for (size_t i = 0; i < count; i++)
		{
			CLAY(CLAY_IDI("fvwm_eval_child", (uint32_t)i), {
				.layout = {
					.sizing = {
						.width = CLAY_SIZING_GROW(copied[i]),
						.height = CLAY_SIZING_FIXED(height)
					}
				}
			}) {}
		}
	}
	(void)Clay_EndLayout(0);
	for (size_t i = 0; i < count; i++)
	{
		Clay_ElementId id = CLAY_IDI("fvwm_eval_child", (uint32_t)i);
		Clay_ElementData data = Clay_GetElementData(id);

		boxes[i] = (clay_layout_eval_box){id.id, data.found,
			data.boundingBox.x, data.boundingBox.y,
			data.boundingBox.width, data.boundingBox.height};
	}
	Clay_SetCurrentContext(previous);
	valid = !state->failed && clay_layout_eval_widths(available, height,
		boxes, count, prepared);
	for (size_t i = 0; valid && i < count; i++)
		valid = prepared[i] >= copied[i];
	if (!valid)
	{
		state->failed = true;
		return CLAY_LAYOUT_EVAL_ERROR;
	}
	memcpy(widths, prepared, count * sizeof(*widths));
	return CLAY_LAYOUT_EVAL_READY;
}
