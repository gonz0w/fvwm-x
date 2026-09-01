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

#include "libs/geometry_transition.h"
#include "libs/monitor_ownership.h"
#include "libs/monitor_scale.h"

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <X11/Xutil.h>

static void
test_normal_plan_logical_seven_at_five_quarters_is_nine(void)
{
	size_rect		   logical     = {.width = 7, .height = 7};
	size_rect		   src	       = {.width = 7, .height = 7};
	monitor_scale_factor	   dest_factor = {5};
	size_borders		   dest_chrome = {0};
	icccm_size_hints	   hints       = {
		  .win_gravity = NorthWestGravity,
	};
	rectangle		   dest_frame;
	geometry_transition_status status;
	int			   expected = 0;

	assert(monitor_scale_apply(7, dest_factor, &expected)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(expected == 9);

	dest_frame.x	  = 100;
	dest_frame.y	  = 200;
	dest_frame.width  = 0;
	dest_frame.height = 0;
	status		  = geometry_transition_plan(
	      GEOMETRY_TRANSITION_NORMAL, logical, src, true, dest_factor,
	      &dest_chrome, NULL, &dest_frame);
	assert(status == GEOMETRY_TRANSITION_OK);
	assert(dest_frame.x == 100);
	assert(dest_frame.y == 200);
	assert(dest_frame.width == 9);
	assert(dest_frame.height == 9);

	dest_frame.x	  = 100;
	dest_frame.y	  = 200;
	dest_frame.width  = 1;
	dest_frame.height = 1;
	status		  = geometry_transition_plan(
	      GEOMETRY_TRANSITION_NORMAL, logical, src, true, dest_factor,
	      &dest_chrome, &hints, &dest_frame);
	assert(status == GEOMETRY_TRANSITION_OK);
	assert(dest_frame.x == 100);
	assert(dest_frame.y == 200);
	assert(dest_frame.width == 9);
	assert(dest_frame.height == 9);
}

static void
test_repeated_plan_from_stored_logical_seven(void)
{
	const unsigned int quarters[] = {5, 6, 7, 8};
	size_rect	   logical    = {.width = 7, .height = 7};
	size_rect	   src	      = {.width = 7, .height = 7};
	size_borders	   dest_chrome = {0};
	unsigned int	   i;
	int		   iteration;

	for (i = 0; i < sizeof(quarters) / sizeof(quarters[0]); i++) {
		monitor_scale_factor dest_factor = {quarters[i]};
		int		     expected	 = 0;

		assert(monitor_scale_apply(7, dest_factor, &expected)
		       == MONITOR_SCALE_CONVERSION_OK);
		for (iteration = 0; iteration < 1000; iteration++) {
			rectangle		   dest_frame = {
				  .x	  = 100,
				  .y	  = 200,
				  .width  = 0,
				  .height = 0,
			};
			geometry_transition_status status;

			status = geometry_transition_plan(
			    GEOMETRY_TRANSITION_NORMAL, logical, src, true,
			    dest_factor, &dest_chrome, NULL, &dest_frame);
			assert(status == GEOMETRY_TRANSITION_OK);
			assert(dest_frame.width == expected);
			assert(dest_frame.height == expected);
			assert(logical.width == 7);
			assert(logical.height == 7);
		}
	}
}

static void
test_increment_snap_does_not_rewrite_logical(void)
{
	size_rect		   logical     = {.width = 7, .height = 7};
	size_rect		   src	       = {.width = 7, .height = 7};
	monitor_scale_factor	   dest_factor = {5};
	size_borders		   dest_chrome = {0};
	icccm_size_hints	   hints       = {
		  .flags      = PResizeInc,
		  .width_inc  = 10,
		  .height_inc = 10,
	};
	rectangle		   dest_frame;
	geometry_transition_status status;
	int			   converted = 0;
	int			   iteration;

	assert(monitor_scale_apply(7, dest_factor, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 9);

	for (iteration = 0; iteration < 2; iteration++) {
		dest_frame.x	  = 100;
		dest_frame.y	  = 200;
		dest_frame.width  = 0;
		dest_frame.height = 0;
		status		  = geometry_transition_plan(
		      GEOMETRY_TRANSITION_NORMAL, logical, src, true,
		      dest_factor, &dest_chrome, &hints, &dest_frame);
		assert(status == GEOMETRY_TRANSITION_OK);
		assert(dest_frame.width != converted);
		assert(logical.width == 7);
		assert(logical.height == 7);
	}
}

static void
test_ineligible_keeps_src_client_and_adds_dest_chrome(void)
{
	size_rect		   logical     = {.width = 7, .height = 7};
	size_rect		   src	       = {.width = 80, .height = 60};
	monitor_scale_factor	   dest_factor = {5};
	size_borders		   dest_chrome = {
		  .total_size = {.width = 8, .height = 16},
	};
	rectangle		   dest_frame = {
		  .x	  = 100,
		  .y	  = 200,
		  .width  = 0,
		  .height = 0,
	};
	geometry_transition_status status;

	status = geometry_transition_plan(GEOMETRY_TRANSITION_NORMAL,
	    logical, src, false, dest_factor, &dest_chrome, NULL,
	    &dest_frame);
	assert(status == GEOMETRY_TRANSITION_INELIGIBLE);
	assert(dest_frame.width - dest_chrome.total_size.width == 80);
	assert(dest_frame.height - dest_chrome.total_size.height == 60);
	assert(dest_frame.width == 88);
	assert(dest_frame.height == 76);
}

static void
test_overflow_zeros_dest_frame(void)
{
	size_rect		   logical = {
		  .width = INT_MAX, .height = INT_MAX};
	size_rect		   src	       = {.width = 7, .height = 7};
	monitor_scale_factor	   dest_factor = {5};
	size_borders		   dest_chrome = {0};
	rectangle		   dest_frame  = {
		  .x	  = 100,
		  .y	  = 200,
		  .width  = 99,
		  .height = 99,
	};
	geometry_transition_status status;

	status = geometry_transition_plan(GEOMETRY_TRANSITION_NORMAL,
	    logical, src, true, dest_factor, &dest_chrome, NULL,
	    &dest_frame);
	assert(status == GEOMETRY_TRANSITION_OVERFLOW);
	assert(dest_frame.width == 0);
	assert(dest_frame.height == 0);
}

static void
test_factor_outside_quarters_is_invalid(void)
{
	size_rect		   logical     = {.width = 7, .height = 7};
	size_rect		   src	       = {.width = 7, .height = 7};
	size_borders		   dest_chrome = {0};
	rectangle		   dest_frame;
	geometry_transition_status status;

	dest_frame.x	  = 100;
	dest_frame.y	  = 200;
	dest_frame.width  = 99;
	dest_frame.height = 99;
	status		  = geometry_transition_plan(
	      GEOMETRY_TRANSITION_NORMAL, logical, src, true,
	      (monitor_scale_factor){3}, &dest_chrome, NULL, &dest_frame);
	assert(status == GEOMETRY_TRANSITION_INVALID_ARGUMENT);
	assert(dest_frame.width == 0);
	assert(dest_frame.height == 0);

	dest_frame.width  = 99;
	dest_frame.height = 99;
	status		  = geometry_transition_plan(
	      GEOMETRY_TRANSITION_NORMAL, logical, src, true,
	      (monitor_scale_factor){17}, &dest_chrome, NULL, &dest_frame);
	assert(status == GEOMETRY_TRANSITION_INVALID_ARGUMENT);
	assert(dest_frame.width == 0);
	assert(dest_frame.height == 0);

	dest_frame.width  = 99;
	dest_frame.height = 99;
	status		  = geometry_transition_plan(
	      GEOMETRY_TRANSITION_NORMAL, logical, src, true,
	      (monitor_scale_factor){5}, NULL, NULL, &dest_frame);
	assert(status == GEOMETRY_TRANSITION_INVALID_ARGUMENT);
	assert(dest_frame.width == 0);
	assert(dest_frame.height == 0);
}

static void
test_maximized_dest_equals_work_rect(void)
{
	size_rect		   logical     = {.width = 7, .height = 7};
	size_rect		   src	       = {.width = 7, .height = 7};
	monitor_scale_factor	   dest_factor = {5};
	size_borders		   dest_chrome = {0};
	rectangle		   dest_frame  = {
		  .x	  = -1920,
		  .y	  = 0,
		  .width  = 1920,
		  .height = 1080,
	};
	geometry_transition_status status;

	status = geometry_transition_plan(GEOMETRY_TRANSITION_MAXIMIZED,
	    logical, src, true, dest_factor, &dest_chrome, NULL,
	    &dest_frame);
	assert(status == GEOMETRY_TRANSITION_OK
	       || status == GEOMETRY_TRANSITION_NOOP);
	assert(dest_frame.x == -1920);
	assert(dest_frame.y == 0);
	assert(dest_frame.width == 1920);
	assert(dest_frame.height == 1080);
}

static void
test_fullscreen_dest_equals_monitor_rect(void)
{
	size_rect		   logical     = {.width = 7, .height = 7};
	size_rect		   src	       = {.width = 7, .height = 7};
	monitor_scale_factor	   dest_factor = {5};
	size_borders		   dest_chrome = {
		  .total_size = {.width = 10, .height = 20},
	};
	rectangle		   dest_frame  = {
		  .x	  = 0,
		  .y	  = 0,
		  .width  = 1920,
		  .height = 1080,
	};
	geometry_transition_status status;

	status = geometry_transition_plan(GEOMETRY_TRANSITION_FULLSCREEN,
	    logical, src, true, dest_factor, &dest_chrome, NULL,
	    &dest_frame);
	assert(status == GEOMETRY_TRANSITION_OK);
	assert(dest_frame.x == 0);
	assert(dest_frame.y == 0);
	assert(dest_frame.width == 1920);
	assert(dest_frame.height == 1080);
}

static void
test_restore_uses_stored_logical_plus_dest_chrome(void)
{
	size_rect		   logical     = {.width = 7, .height = 7};
	size_rect		   src	       = {.width = 7, .height = 7};
	monitor_scale_factor	   dest_factor = {5};
	size_borders		   dest_chrome = {
		  .total_size = {.width = 10, .height = 20},
	};
	rectangle		   dest_frame  = {
		  .x	  = 100,
		  .y	  = 200,
		  .width  = 0,
		  .height = 0,
	};
	geometry_transition_status status;

	status = geometry_transition_plan(GEOMETRY_TRANSITION_RESTORE,
	    logical, src, true, dest_factor, &dest_chrome, NULL,
	    &dest_frame);
	assert(status == GEOMETRY_TRANSITION_OK);
	assert(dest_frame.width - dest_chrome.total_size.width == 9);
	assert(dest_frame.height - dest_chrome.total_size.height == 9);
	assert(dest_frame.width == 19);
	assert(dest_frame.height == 29);
}

static void
test_restore_echo_keeps_stored_logical_seven(void)
{
	size_rect		   logical     = {.width = 7, .height = 7};
	size_rect		   src	       = {.width = 7, .height = 7};
	monitor_scale_factor	   dest_factor = {5};
	size_borders		   dest_chrome = {0};
	icccm_size_hints	   hints       = {
		  .flags      = PResizeInc,
		  .width_inc  = 10,
		  .height_inc = 10,
	};
	rectangle		   dest_frame  = {
		  .x	  = 100,
		  .y	  = 200,
		  .width  = 0,
		  .height = 0,
	};
	geometry_transition_status status;
	int			   converted = 0;
	int			   removed   = 0;
	int			   last_sent_w;
	int			   last_sent_h;

	assert(monitor_scale_apply(7, dest_factor, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	status = geometry_transition_plan(GEOMETRY_TRANSITION_RESTORE,
	    logical, src, true, dest_factor, &dest_chrome, &hints,
	    &dest_frame);
	assert(status == GEOMETRY_TRANSITION_OK);
	assert(dest_frame.width != converted);
	assert(monitor_scale_remove(dest_frame.width, dest_factor, &removed)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(removed != 7);
	last_sent_w = dest_frame.width;
	last_sent_h = dest_frame.height;
	assert(last_sent_w == dest_frame.width);
	assert(last_sent_h == dest_frame.height);
	assert(logical.width == 7);
	assert(logical.height == 7);
}

static void
test_grab_offset_apply_from_source(void)
{
	monitor_scale_factor dest_factor   = {5};
	int		     dest_offset_x = 0;
	int		     dest_offset_y = 0;
	int		     origin_x;
	int		     origin_y;

	assert(monitor_scale_apply(8, dest_factor, &dest_offset_x)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(monitor_scale_apply(8, dest_factor, &dest_offset_y)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(dest_offset_x == 10);
	assert(dest_offset_y == 10);
	origin_x = 100 + dest_offset_x;
	origin_y = 50 + dest_offset_y;
	assert(origin_x == 110);
	assert(origin_y == 60);
}

static void
fill_seam_monitors(monitor_topology_monitor monitors[2])
{
	monitors[0] = (monitor_topology_monitor){
	    .name_atom = 1,
	    .name      = "HDMI-1",
	    .x	       = 0,
	    .y	       = 0,
	    .width     = 1000,
	    .height    = 1000,
	};
	monitors[1] = (monitor_topology_monitor){
	    .name_atom = 2,
	    .name      = "DP-1",
	    .x	       = 1000,
	    .y	       = 0,
	    .width     = 1000,
	    .height    = 1000,
	};
}

static void
assert_owner(const monitor_ownership_result *result, Atom name_atom,
    const char *name, monitor_ownership_reason reason)
{
	assert(result->name_atom == name_atom);
	assert(result->name != NULL);
	assert(strcmp(result->name, name) == 0);
	assert(result->reason == reason);
}

static void
test_pointer_one_by_one_freeze_ignores_post_resize_frame(void)
{
	monitor_topology_monitor monitors[2];
	rectangle		 pointer = {
	    .x	    = 500,
	    .y	    = 500,
	    .width  = 1,
	    .height = 1,
	};
	rectangle		 keep_strip = {
	    .x	    = 954,
	    .y	    = 0,
	    .width  = 100,
	    .height = 1,
	};
	rectangle		 steal_strip = {
	    .x	    = 954,
	    .y	    = 0,
	    .width  = 101,
	    .height = 1,
	};
	rectangle		 post_resize = {
	    .x	    = 891,
	    .y	    = 0,
	    .width  = 200,
	    .height = 100,
	};
	monitor_ownership_result result;
	monitor_ownership_status status;

	fill_seam_monitors(monitors);

	status = monitor_ownership_resolve(&pointer, monitors, 2, 1,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);
	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(
	    &result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_INCUMBENT);

	status = monitor_ownership_resolve(&keep_strip, monitors, 2, 1,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);
	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(
	    &result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_INCUMBENT);
	assert(result.extra_overlap == MONITOR_OWNERSHIP_HYSTERESIS_PX);
	assert(result.extra_overlap == 8);

	status = monitor_ownership_resolve(&steal_strip, monitors, 2, 1,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);
	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(&result, 2, "DP-1", MONITOR_OWNERSHIP_REASON_OVERLAP);
	assert(result.extra_overlap == 9);

	status = monitor_ownership_resolve(&post_resize, monitors, 2, 2,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);
	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(&result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_OVERLAP);
}

int
main(void)
{
	test_normal_plan_logical_seven_at_five_quarters_is_nine();
	test_repeated_plan_from_stored_logical_seven();
	test_increment_snap_does_not_rewrite_logical();
	test_ineligible_keeps_src_client_and_adds_dest_chrome();
	test_overflow_zeros_dest_frame();
	test_factor_outside_quarters_is_invalid();
	test_maximized_dest_equals_work_rect();
	test_fullscreen_dest_equals_monitor_rect();
	test_restore_uses_stored_logical_plus_dest_chrome();
	test_restore_echo_keeps_stored_logical_seven();
	test_grab_offset_apply_from_source();
	test_pointer_one_by_one_freeze_ignores_post_resize_frame();

	return 0;
}
