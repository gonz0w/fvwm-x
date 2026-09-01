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

int
main(void)
{
	test_normal_plan_logical_seven_at_five_quarters_is_nine();

	return 0;
}
