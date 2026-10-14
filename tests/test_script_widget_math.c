/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "modules/FvwmScript/script_widget_math.h"
#include <assert.h>
#include <limits.h>

static void test_startup_ranges(void)
{
	assert(script_widget_initial_maximum(20, 10, 50, false) == 70);
	assert(script_widget_initial_maximum(20, 20, 50, false) == 20);
	assert(script_widget_initial_maximum(20, 20, 10, true) == 30);
	assert(script_widget_initial_maximum(20, 100, 10, true) == 100);
	assert(script_widget_initial_maximum(INT_MAX-2, 0, 10, true) == INT_MAX);
	assert(script_widget_initial_maximum(INT_MAX, INT_MAX, 10, true) == INT_MAX);
	assert(script_widget_initial_maximum(INT_MIN, INT_MIN, 10, true) == INT_MIN+10);
}

static void test_drawing_boundaries(void)
{
	assert(script_widget_position(25, 0, 100, 120, 4, false) == 29);
	assert(script_widget_position(25, 0, 100, 136, 36, true) == 75);
	assert(script_widget_position(-20, 0, 100, 120, 4, false) == 0);
	assert(script_widget_position(200, 0, 100, 120, 4, false) == 116);
	assert(script_widget_position(99, 10, 10, 120, 4, false) == 0);
	assert(script_widget_position(99, 20, 10, 120, 4, true) == 0);
	assert(script_widget_position(50, 0, 100, 4, 4, false) == 0);
	assert(script_widget_position(50, 0, 100, INT_MIN, 4, false) == 0);
	assert(script_widget_position(INT_MAX, INT_MIN, INT_MAX,
		INT_MAX, 0, false) == INT_MAX);
	assert(script_widget_position(INT_MIN, INT_MIN, INT_MAX,
		INT_MAX, 0, true) == INT_MAX);
	assert(script_widget_position(0, INT_MIN, INT_MAX, 120, 4, false) == 58);
	assert(script_widget_position(INT_MAX, INT_MIN, INT_MAX, 120, 4, true) == 0);
}

static void test_legacy_rounding(void)
{
	/* Compare against the old expression where its int arithmetic is safe.
	 * The intermediate division matters; a single ratio changes some values. */
	for (int extent = 40; extent <= 180; extent++)
	{
		for (int position = 15; position <= extent-21; position++)
		{
			int forward = (position-15)*extent/(extent-36)*60/extent-20;
			int reverse = (position-15)*extent/(extent-36)*-60/extent+40;
			assert(script_widget_pointer_value(position, extent, -20, 40,
				false) == forward);
			assert(script_widget_pointer_value(position, extent, -20, 40,
				true) == reverse);
		}
	}
	assert(script_widget_pointer_position(INT_MIN, INT_MAX, 120) == 15);
	assert(script_widget_pointer_position(INT_MAX, INT_MIN, 120) == 99);
	assert(script_widget_pointer_value(INT_MAX, INT_MAX, INT_MIN, INT_MAX,
		false) == INT_MAX);
	assert(script_widget_pointer_value(INT_MIN, INT_MAX, INT_MIN, INT_MAX,
		true) == INT_MAX);
	assert(script_widget_pointer_value(50, 36, 10, 100, false) == 10);
	assert(script_widget_pointer_value(50, 120, 10, 10, true) == 10);
	assert(script_widget_pointer_value(50, 120, 20, 10, false) == 20);
}

static void test_wrapping(void)
{
	assert(script_widget_step(4, 0, 10, true) == 5);
	assert(script_widget_step(4, 0, 10, false) == 3);
	assert(script_widget_step(10, 0, 10, true) == 0);
	assert(script_widget_step(0, 0, 10, false) == 10);
	assert(script_widget_step(INT_MAX, INT_MIN, INT_MAX, true) == INT_MIN);
	assert(script_widget_step(INT_MIN, INT_MIN, INT_MAX, false) == INT_MAX);
	assert(script_widget_step(10, 10, 10, true) == 10);
	assert(script_widget_step(10, 10, 10, false) == 10);
}

static void test_list_geometry(void)
{
	int position, size;

	assert(script_widget_dimension(INT_MIN) == 0);
	assert(script_widget_dimension(INT_MAX) == 65535);
	assert(script_widget_dimension(120) == 120);
	assert(script_list_row_height(INT_MIN) == 1);
	assert(script_list_row_height(INT_MAX) == INT_MAX);
	assert(script_list_first(INT_MIN, 12, 3) == 1);
	assert(script_list_first(INT_MAX, 12, 3) == 10);
	script_list_thumb(4, 12, 3, 48, &position, &size);
	assert(position == 10 && size == 18);
	script_list_thumb(INT_MIN, 0, 0, 0, &position, &size);
	assert(position == 0 && size == 0);
	script_list_thumb(INT_MAX, INT_MAX, INT_MAX, INT_MAX, &position, &size);
	assert(position == 0 && size == INT_MAX);
	script_list_thumb(INT_MAX, INT_MAX, 1, INT_MAX, &position, &size);
	assert(position >= 0 && size >= 8);
	assert((int64_t)position + size <= INT_MAX);
	assert(script_list_pointer_first(INT_MAX, INT_MIN, 48, 12, 3) == 10);
	assert(script_list_pointer_first(INT_MIN, INT_MAX, 48, 12, 3) == 1);
	assert(script_list_pointer_first(10, 0, 0, 12, 3) == 1);
	assert(script_list_move(INT_MAX, INT_MAX, 12, 3) == 10);
	assert(script_list_move(INT_MIN, INT_MIN, 12, 3) == 1);
	assert(script_list_cell(INT_MAX, INT_MAX, 12) == 0);
	assert(script_list_cell(1, 2, 12) == 3);
	/* The thumb always stays within the track across a full small list. */
	for (int first = -2; first < 20; first++)
	{
		script_list_thumb(first, 12, 3, 48, &position, &size);
		assert(position >= 0 && size > 0 && position + size <= 48);
	}
}

int main(void)
{
	test_startup_ranges();
	test_drawing_boundaries();
	test_legacy_rounding();
	test_wrapping();
	test_list_geometry();
	return 0;
}
