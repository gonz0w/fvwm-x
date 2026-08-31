/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

#include "config.h"

#include "libs/wm_ui_metrics.h"

#include <assert.h>
#include <stdbool.h>

static void
test_source_handle_width_scales_at_1x_and_2x(void)
{
	monitor_scale_factor one_x = {4};
	monitor_scale_factor two_x = {8};
	int expected = 99;
	int scaled = 99;

	assert(monitor_scale_apply(7, one_x, &expected)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(expected == 7);
	assert(wm_ui_scale_px(7, one_x, &scaled)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(scaled == 7);
	assert(scaled == expected);

	assert(monitor_scale_apply(7, two_x, &expected)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(expected == 14);
	assert(wm_ui_scale_px(7, two_x, &scaled)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(scaled == 14);
	assert(scaled == expected);
}

static void
test_cache_key_misses_across_quarters_and_generation(void)
{
	wm_ui_cache_key one_x = {
	    .scale_quarters = 4,
	    .target_width = 16,
	    .target_height = 16,
	    .generation = WM_UI_RESOURCE_GENERATION_INITIAL,
	};
	wm_ui_cache_key two_x = one_x;
	wm_ui_cache_key same = one_x;
	wm_ui_cache_key next_generation = one_x;
	wm_ui_resource_generation current;

	two_x.scale_quarters = 8;
	next_generation.generation = 2U;

	assert(!wm_ui_cache_key_equal(&one_x, &two_x));
	assert(wm_ui_cache_key_equal(&one_x, &same));
	assert(!wm_ui_cache_key_equal(&one_x, &next_generation));

	current = wm_ui_resource_generation_current();
	assert(current.generation == WM_UI_RESOURCE_GENERATION_INITIAL);
}

int
main(void)
{
	test_source_handle_width_scales_at_1x_and_2x();
	test_cache_key_misses_across_quarters_and_generation();

	return 0;
}
