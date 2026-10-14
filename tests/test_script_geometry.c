/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "modules/FvwmScript/script_geometry.h"
#include "libs/font_geometry.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>

static void
test_layout(void)
{
	script_layout_input input = {
		.width = 100, .height = 30, .x = 10, .y = 20,
		.font_height = 13, .text_width = 60, .second_text_width = 20,
		.icon_width = 40, .icon_height = 25, .has_title = true,
		.menu_x = 125
	};
	const int expected[][4] = {
		{100, 30, 10, 20}, {80, 18, 10, 20},
		{100, 30, 10, 20}, {90, 18, 10, 20},
		{100, 23, 10, 20}, {100, 56, 10, 20},
		{150, 56, 10, 20}, {94, 25, 10, 20},
		{100, 30, 10, 20}, {19, 34, 10, 20},
		{100, 30, 10, 20}, {100, 30, 10, 20},
		{100, 30, 10, 20}, {108, 62, 10, 20},
		{66, 17, 125, 2}
	};
	script_layout_geometry geometry;

	for (int type = SCRIPT_LAYOUT_PUSH; type <= SCRIPT_LAYOUT_MENU; type++)
	{
		input.type = (script_layout_type)type;
		assert(script_geometry_layout(&input, &geometry));
		assert(geometry.width == expected[type][0]);
		assert(geometry.height == expected[type][1]);
		assert(geometry.x == expected[type][2]);
		assert(geometry.y == expected[type][3]);
	}
	input.type = SCRIPT_LAYOUT_CHECK;
	input.text_width = 3;
	assert(script_geometry_layout(&input, &geometry));
	assert(geometry.width == 4); /* Legacy truncation of 4.5. */
	input.text_width = 0;
	assert(!script_geometry_layout(&input, &geometry));
	assert(geometry.width == 0 && geometry.height == 0);
	input.text_width = INT_MAX;
	assert(!script_geometry_layout(&input, &geometry));
	input.text_width = INT_MIN;
	assert(!script_geometry_layout(&input, &geometry));
	input.type = SCRIPT_LAYOUT_HSCROLL;
	input.font_height = INT_MAX;
	assert(!script_geometry_layout(&input, &geometry));
	input.type = SCRIPT_LAYOUT_PUSH;
	input.has_icon = true;
	input.icon_height = INT_MAX;
	assert(!script_geometry_layout(&input, &geometry));
	input.font_height = 13;
	input.text_width = 60;
	input.icon_height = 25;
	assert(script_geometry_layout(&input, &geometry));
	assert(geometry.width == 100 && geometry.height == 71);
	input.type = SCRIPT_LAYOUT_ITEM;
	input.height = 300;
	assert(script_geometry_layout(&input, &geometry));
	assert(geometry.height == 40); /* Fixed combined title/icon height. */
	input.type = SCRIPT_LAYOUT_MENU;
	input.menu_x = INT64_MAX;
	assert(!script_geometry_layout(&input, &geometry));
	input.menu_x = 32767;
	assert(script_geometry_layout(&input, &geometry));
	input.menu_x = 32768;
	assert(!script_geometry_layout(&input, &geometry));
	assert(!script_geometry_layout(NULL, &geometry));
	assert(!script_geometry_layout(&input, NULL));
}

int
main(void)
{
	int width = -1, height = -1;

	assert(script_geometry_size(1, 1));
	assert(script_geometry_size(65535, 65535));
	assert(!script_geometry_size(0, 1));
	assert(!script_geometry_size(1, -1));
	assert(!script_geometry_size(65536, 1));
	assert(!script_geometry_size(1, INT64_MAX));
	assert(script_geometry_position(-32768, 32767));
	assert(script_geometry_position(32767, -32768));
	assert(!script_geometry_position(-32769, 0));
	assert(!script_geometry_position(0, 32768));
	assert(!script_geometry_position(INT64_MIN, INT64_MAX));
	assert(script_geometry_startup_size(0, 0, &width, &height));
	assert(width == 1 && height == 1);
	assert(script_geometry_startup_size(100, 30, &width, &height));
	assert(width == 100 && height == 30);
	assert(!script_geometry_startup_size(-1, 20, &width, &height));
	assert(width == 0 && height == 0);
	assert(!script_geometry_startup_size(0, 65536, &width, &height));
	assert(width == 0 && height == 0);
	height = 42;
	assert(!script_geometry_startup_size(1, 1, NULL, &height));
	assert(height == 0);
	assert(font_geometry_int((int64_t)INT_MAX + 1) == INT_MAX);
	assert(font_geometry_int((int64_t)INT_MIN - 1) == INT_MIN);
	assert(font_geometry_int(100) == 100);
	assert(font_geometry_shadow(13, 10, 3, 2, 1,
		true, true, true, true));
	assert(!font_geometry_shadow(13, 10, 3, INT_MAX, INT_MAX,
		true, true, true, true));
	assert(!font_geometry_shadow(13, 10, 3, INT_MAX, 0,
		false, true, false, true));
	assert(!font_geometry_shadow(13, 10, 3, -1, 0,
		false, true, false, true));
	test_layout();
	return 0;
}
