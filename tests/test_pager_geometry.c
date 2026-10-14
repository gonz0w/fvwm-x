/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmPager/pager_geometry.h"
#include "libs/module_monitor.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void test_desktop_preparation(void)
{
	pager_desktop_geometry result;
	char text[128];

	assert(pager_prepare_desktop("2 3", 800, 600, &result));
	assert(result.width == 1600 && result.height == 1800);
	assert(result.max_x == 800 && result.max_y == 1200);
	assert(result.x_pages == 2 && result.y_pages == 3);
	assert(pager_prepare_desktop("0 -7", 800, 600, &result));
	assert(result.max_x == 0 && result.max_y == 0);
	assert(result.x_pages == 1 && result.y_pages == 1);
	assert(pager_prepare_desktop("1+2suffix", 800, 600, &result));
	assert(result.x_pages == 1 && result.y_pages == 2);
	snprintf(text, sizeof(text), "%d 1", INT_MAX / 800);
	assert(pager_prepare_desktop(text, 800, 600, &result));
	snprintf(text, sizeof(text), "%d 1", INT_MAX / 800 + 1);
	assert(!pager_prepare_desktop(text, 800, 600, &result));
	assert(result.width == 0 && result.max_x == 0);
	snprintf(text, sizeof(text), "1 %d", INT_MAX / 600 + 1);
	assert(!pager_prepare_desktop(text, 800, 600, &result));
	snprintf(text, sizeof(text), "%d %d", INT_MAX, INT_MAX);
	assert(pager_prepare_desktop(text, 1, 1, &result));
	assert(result.width == INT_MAX && result.height == INT_MAX);
	snprintf(text, sizeof(text), "%d %d", INT_MIN, INT_MIN);
	assert(pager_prepare_desktop(text, 800, 600, &result));
	assert(result.x_pages == 1 && result.y_pages == 1);
	const char *bad[] = {"", "1", "x 2", "1 x", "2147483648 1",
		"-2147483649 1", "9999999999999999999999999999999999 1"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		assert(!pager_prepare_desktop(bad[i], 800, 600, &result));
		assert(result.width == 0 && result.height == 0);
	}
	assert(!pager_prepare_desktop(NULL, 800, 600, &result));
	assert(!pager_prepare_desktop("1 1", 0, 600, &result));
	assert(!pager_prepare_desktop("1 1", 800, -1, &result));
	assert(!pager_prepare_desktop("1 1", 800, 600, NULL));
}

static void test_integer_messages(void)
{
	int values[14];
	pager_monitor_geometry monitor;

	assert(module_parse_decimal_integers("2 -1 suffix", values, 2));
	assert(values[0] == 2 && values[1] == -1);
	assert(module_parse_decimal_integers("1+2suffix", values, 2));
	assert(values[0] == 1 && values[1] == 2);
	assert(!module_parse_decimal_integers(NULL, values, 2));
	assert(!module_parse_decimal_integers("1 1", NULL, 2));
	assert(pager_prepare_monitor(
		"0 2 3 5 6 800 1200 -1 800 600 0 0 0 0", &monitor));
	assert(monitor.desk == -1 && monitor.viewport_x == 5);
	assert(monitor.desktop.width == 1600 &&
		monitor.desktop.height == 1800);
	assert(monitor.desktop.max_x == 800 &&
		monitor.desktop.max_y == 1200);
	assert(pager_prepare_monitor(
		"2 0 -1 0 0 0 0 0 800 600 -1 -2 -3 -4suffix", &monitor));
	assert(monitor.flags == 2 && monitor.desktop.x_pages == 1 &&
		monitor.desktop.y_pages == 1);
	const char *bad[] = {"", "0 1 1 0 0 0 0 0 800 600 0 0 0",
		"0 1 1 0 0 0 0 0 800 600 0 0 0 x",
		"2147483648 1 1 0 0 0 0 0 800 600 0 0 0 0",
		"0 2147483647 1 0 0 0 0 0 800 600 0 0 0 0",
		"0 1 2147483647 0 0 0 0 0 800 600 0 0 0 0",
		"0 1 1 0 0 0 0 0 0 600 0 0 0 0",
		"0 1 1 0 0 0 0 0 800 -1 0 0 0 0"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		assert(!pager_prepare_monitor(bad[i], &monitor));
		assert(monitor.screen_width == 0 && monitor.desktop.width == 0);
	}
	const char *short_bad[] = {"", "1", "x 1", "1 x", "2147483648 1",
		"1 -2147483649"};
	for (size_t i = 0; i < sizeof(short_bad) / sizeof(short_bad[0]); i++)
	{
		assert(!module_parse_decimal_integers(short_bad[i], values, 2));
	}
	assert(!pager_prepare_monitor(NULL, &monitor));
	assert(!pager_prepare_monitor("", NULL));
}

static void test_grid_and_scale(void)
{
	int rows = 71, columns = 72;
	unsigned int scale = 32;
	const int cases[][5] = {
		{5, -1, -1, 1, 5}, {5, -1, 2, 3, 2},
		{5, 2, -1, 2, 3}, {5, 0, -1, 1, 5},
		{5, 0, 0, 5, 1}, {5, 1, 2, 3, 2},
		{5, 4, 3, 4, 3}, {1, INT_MAX, INT_MAX, INT_MAX, INT_MAX},
		{65536, -1, 1, 65536, 1}, {INT_MAX, 0, 1, INT_MAX, 1}};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		assert(pager_prepare_grid(cases[i][0], cases[i][1], cases[i][2],
			&rows, &columns));
		assert(rows == cases[i][3] && columns == cases[i][4]);
	}
	rows = 71;
	columns = 72;
	assert(!pager_prepare_grid(0, 1, 1, &rows, &columns));
	assert(rows == 71 && columns == 72);
	assert(!pager_prepare_grid(1, 1, 1, NULL, &columns));
	assert(!pager_prepare_grid(1, 1, 1, &rows, NULL));
	assert(pager_prepare_scale("16suffix", &scale) && scale == 16);
	assert(pager_prepare_scale("2147483647", &scale));
	assert(scale == INT_MAX);
	const char *bad[] = {"0", "-1", "", "x", "2147483648"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		assert(!pager_prepare_scale(bad[i], &scale));
		assert(scale == INT_MAX);
	}
	assert(!pager_prepare_scale(NULL, &scale));
	assert(!pager_prepare_scale("16", NULL));
}

static void test_rendering_dimensions(void)
{
	int width = 71, height = 72, x, span;

	assert(pager_prepare_window_size(0, 0, 2, 3, 10, 2, 3,
		800, 600, 32, &width, &height));
	assert(width == 153 && height == 134);
	assert(pager_prepare_window_size(200, 0, 2, 3, 10, 2, 3,
		800, 600, 32, &width, &height));
	assert(width == 200 && height == 172);
	assert(pager_prepare_window_size(0, 100, 2, 3, 10, 2, 3,
		800, 600, 32, &width, &height));
	assert(width == 112 && height == 100);
	assert(pager_prepare_window_size(0, 0, INT_MAX, INT_MAX,
		INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX,
		&width, &height));
	assert(width == 65535 && height == 65535);
	assert(pager_prepare_window_size(INT_MAX, 0, INT_MAX, INT_MAX,
		INT_MAX, 1, 1, 1, INT_MAX, 1, &width, &height));
	assert(width == 65535 && height == 65535);
	assert(pager_prepare_window_size(0, INT_MAX, INT_MAX, INT_MAX,
		0, 1, 1, INT_MAX, 1, 1, &width, &height));
	assert(width == 65535 && height == 65535);
	width = 71;
	height = 72;
	assert(!pager_prepare_window_size(0, 0, 1, 1, 0, 1, 1,
		800, 600, 0, &width, &height));
	assert(width == 71 && height == 72);
	assert(pager_prepare_cell_size(303, 222, 2, 3, 10, &width, &height));
	assert(width == 100 && height == 100);
	assert(pager_prepare_cell_size(60000, 60000, INT_MAX, INT_MAX,
		INT_MAX, &width, &height));
	assert(width == 1 && height == 1);
	assert(pager_round_icon_size(100, 2) == 101);
	assert(pager_round_icon_size(100, INT_MAX) == 65535);
	assert(pager_round_icon_size(INT_MAX, 1) == 65535);
	assert(pager_pixel_size(0) == 1 && pager_pixel_size(INT64_MAX) == 65535);
	assert(pager_pixel_position(INT64_MIN) == -32768);
	assert(pager_pixel_position(INT64_MAX) == 32767);
	assert(pager_prepare_viewport_axis(800, 800, 200, 1600, &x, &span));
	assert(x == 100 && span == 100);
	assert(pager_prepare_viewport_axis(INT_MAX, INT_MAX,
		65535, INT_MAX, &x, &span));
	assert(x == 32767 && span == 65535);
	assert(pager_prepare_viewport_axis((int64_t)INT_MIN * 2, INT_MAX,
		65535, INT_MAX, &x, &span));
	assert(x == -32768 && span == 65535);
	assert(!pager_prepare_viewport_axis(1, 1, 1, 0, &x, &span));
	assert(x == 0 && span == 1);
}

static void test_window_projection(void)
{
	pager_projection_axis axis = {3200, 1280, 1920, 2, 200, false};
	int x, width;

	for (int tracked = 0; tracked <= 1; tracked++)
	{
		axis.tracked = tracked;
		for (int position = -8000; position < 8000; position += 137)
		{
			int local = position;
			int full = axis.all_pixels * axis.pages;
			if (tracked)
			{
				local -= (axis.all_pixels - axis.monitor_pixels) *
					(position / axis.all_pixels) + axis.monitor_origin;
				full = axis.monitor_pixels * axis.pages;
			}
			int expected = local * axis.scale / full;
			int expected_size = (local + 320) * axis.scale / full - expected;
			assert(pager_project_axis(position, 320, &axis, &x, &width));
			assert(x == expected && width == expected_size);
		}
		for (int position = -500; position <= 500; position += 17)
		{
			int full = axis.all_pixels * axis.pages;
			int offset = 0;
			if (tracked)
			{
				offset = (axis.all_pixels - axis.monitor_pixels) *
					((axis.pages * position) / axis.scale) + axis.monitor_origin;
				full = axis.monitor_pixels * axis.pages;
			}
			assert(pager_restore_axis(position, 40, &axis, &x, &width));
			assert(x == position * full / axis.scale + offset);
			assert(width == 40 * full / axis.scale);
		}
	}
	axis = (pager_projection_axis){INT_MAX, INT_MAX, INT_MAX,
		INT_MAX, 1, true};
	assert(pager_restore_axis(INT_MAX, INT_MAX, &axis, &x, &width));
	assert(x == INT_MAX && width == INT_MAX);
	assert(pager_restore_axis(INT_MIN, INT_MAX, &axis, &x, &width));
	assert(x == INT_MIN && width == INT_MAX);
	axis = (pager_projection_axis){1, 1, 0, 1, 65535, false};
	assert(pager_project_axis(INT_MAX, 320, &axis, &x, &width));
	assert(x == 32767 && width == 65535);
	assert(pager_project_axis(INT_MIN, 240, &axis, &x, &width));
	assert(x == -32768 && width == 65535);
	axis.tracked = true;
	axis.monitor_pixels = 2;
	assert(!pager_project_axis(0, 1, &axis, &x, &width));
	assert(x == -32768 && width == 1);
	assert(!pager_restore_axis(0, 1, &axis, &x, &width));
	assert(x == 0 && width == 1);
	assert(!pager_project_axis(0, 1, NULL, &x, &width));
	assert(!pager_restore_axis(0, 1, &axis, NULL, &width));
	assert(pager_integer_value(INT64_MAX) == INT_MAX);
	assert(pager_integer_value(INT64_MIN) == INT_MIN);
	unsigned int metric = 42;
	assert(pager_prepare_window_metric("0", &metric) && metric == 0);
	assert(pager_prepare_window_metric("2147483647suffix", &metric));
	assert(metric == INT_MAX && pager_minimum_size(metric, metric) == 65535);
	assert(pager_minimum_size(1, 3) == 5);
	assert(pager_minimum_size(0, 3) == 3);
	const char *bad[] = {"", "x", "-1", "2147483648"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		assert(!pager_prepare_window_metric(bad[i], &metric));
		assert(metric == INT_MAX);
	}
}

int main(void)
{
	test_desktop_preparation();
	test_integer_messages();
	test_grid_and_scale();
	test_rendering_dimensions();
	test_window_projection();
	return 0;
}
