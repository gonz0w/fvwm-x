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
#include "libs/monitor_scale.h"

#include <assert.h>
#include <limits.h>
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

int
main(void)
{
	test_normal_plan_logical_seven_at_five_quarters_is_nine();
	test_repeated_plan_from_stored_logical_seven();
	test_increment_snap_does_not_rewrite_logical();
	test_ineligible_keeps_src_client_and_adds_dest_chrome();
	test_overflow_zeros_dest_frame();
	test_factor_outside_quarters_is_invalid();

	return 0;
}
