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

#include "libs/icccm_size_hints.h"
#include "libs/monitor_scale.h"

#include <assert.h>
#include <limits.h>

static void
test_min_max_clamp(void)
{
	icccm_size_hints hints = {
	    .flags	= PMinSize | PMaxSize,
	    .min_width	= 20,
	    .min_height = 10,
	    .max_width	= 40,
	    .max_height = 30,
	};
	int		     width;
	int		     height;
	icccm_constrain_status status;

	width  = 19;
	height = 15;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 20);
	assert(height == 15);

	width  = 21;
	height = 15;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 21);
	assert(height == 15);

	width  = 39;
	height = 15;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 39);
	assert(height == 15);

	width  = 41;
	height = 15;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 40);
	assert(height == 15);

	width  = 10;
	height = 15;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 20);

	width  = 50;
	height = 15;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 40);

	width  = 99;
	height = 99;
	status = icccm_constrain_size(NULL, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_INVALID_ARGUMENT);
	assert(width == 0);
	assert(height == 0);

	width  = 99;
	height = 99;
	status = icccm_constrain_size(&hints, NULL, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_INVALID_ARGUMENT);
	assert(height == 0);

	width  = 99;
	height = 99;
	status = icccm_constrain_size(&hints, &width, NULL, NULL);
	assert(status == ICCCM_CONSTRAIN_INVALID_ARGUMENT);
	assert(width == 0);
}

static void
test_base_inc_grid(void)
{
	icccm_size_hints hints = {
	    .flags	 = PMinSize | PResizeInc,
	    .min_width	 = 10,
	    .min_height	 = 10,
	    .width_inc	 = 8,
	    .height_inc	 = 8,
	};
	int		     width;
	int		     height;
	icccm_constrain_status status;

	width  = 14;
	height = 10;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 18);
	assert(height == 10);

	width  = 13;
	height = 10;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 10);

	hints.flags	 = PMinSize | PMaxSize | PBaseSize | PResizeInc;
	hints.base_width = 4;
	hints.base_height = 4;
	hints.max_width	 = 100;
	hints.max_height = 100;
	width		 = 16;
	height		 = 12;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 20);
	assert(height == 12);
}

static void
test_aspect_with_pbase_subtracts_base(void)
{
	icccm_size_hints hints = {
	    .flags	  = PMinSize | PBaseSize | PAspect,
	    .min_width	  = 4,
	    .min_height	  = 4,
	    .base_width	  = 4,
	    .base_height  = 4,
	    .min_aspect	  = {.x = 2, .y = 1},
	    .max_aspect	  = {.x = 100, .y = 1},
	};
	int		     width  = 20;
	int		     height = 20;
	icccm_constrain_status status;

	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 20);
	assert(height == 12);
}

static void
test_aspect_without_pbase_does_not_use_min(void)
{
	icccm_size_hints hints = {
	    .flags	 = PMinSize | PAspect,
	    .min_width	 = 4,
	    .min_height	 = 4,
	    .min_aspect	 = {.x = 2, .y = 1},
	    .max_aspect	 = {.x = 100, .y = 1},
	};
	int		     width  = 20;
	int		     height = 20;
	icccm_constrain_status status;

	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 20);
	assert(height == 10);
}

static void
test_base_greater_than_min_skips_aspect_base(void)
{
	icccm_size_hints hints = {
	    .flags	  = PMinSize | PBaseSize | PAspect,
	    .min_width	  = 10,
	    .min_height	  = 10,
	    .base_width	  = 20,
	    .base_height  = 20,
	    .min_aspect	  = {.x = 2, .y = 1},
	    .max_aspect	  = {.x = 100, .y = 1},
	};
	int		     width  = 30;
	int		     height = 30;
	icccm_constrain_status status;

	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 30);
	assert(height == 15);
}

static void
test_fixed_size(void)
{
	icccm_size_hints hints = {
	    .flags	= PMinSize | PMaxSize,
	    .min_width	= 50,
	    .min_height = 40,
	    .max_width	= 50,
	    .max_height = 40,
	};
	int		     width;
	int		     height;
	icccm_constrain_status status;

	width  = 100;
	height = 80;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 50);
	assert(height == 40);

	width  = 10;
	height = 10;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 50);
	assert(height == 40);
}

static void
test_gravity_northwest_default(void)
{
	icccm_size_hints hints = {
	    .flags	= PMinSize | PMaxSize,
	    .min_width	= 50,
	    .min_height = 40,
	    .max_width	= 50,
	    .max_height = 40,
	};
	rectangle	     frame  = {.x = 10, .y = 20, .width = 100,
		    .height					= 80};
	int		     width  = 100;
	int		     height = 80;
	icccm_constrain_status status;

	status = icccm_constrain_size(&hints, &width, &height, &frame);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 50);
	assert(height == 40);
	assert(frame.x == 10);
	assert(frame.y == 20);
	assert(frame.width == 50);
	assert(frame.height == 40);
}

static void
test_zero_increment_invalid(void)
{
	icccm_size_hints hints = {
	    .flags	= PResizeInc,
	    .width_inc	= 0,
	    .height_inc = 8,
	};
	int		     width  = 24;
	int		     height = 24;
	icccm_constrain_status status;

	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_INVALID_ARGUMENT);
	assert(width == 0);
	assert(height == 0);

	hints.width_inc	 = 8;
	hints.height_inc = 0;
	width		 = 24;
	height		 = 24;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_INVALID_ARGUMENT);
	assert(width == 0);
	assert(height == 0);
}

static void
test_aspect_overflow_initializes_outputs(void)
{
	icccm_size_hints hints = {
	    .flags	 = PMinSize | PAspect,
	    .min_width	 = 1,
	    .min_height	 = 3,
	    .min_aspect	 = {.x = INT_MAX, .y = 1},
	    .max_aspect	 = {.x = INT_MAX, .y = 1},
	};
	int		     width  = 1;
	int		     height = 3;
	icccm_constrain_status status;

	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OVERFLOW);
	assert(width == 0);
	assert(height == 0);

	hints = (icccm_size_hints){
	    .flags	  = PBaseSize | PResizeInc,
	    .base_width	  = 10,
	    .base_height  = 10,
	    .width_inc	  = INT_MAX,
	    .height_inc	  = 1,
	};
	width  = INT_MAX;
	height = 10;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OVERFLOW);
	assert(width == 0);
	assert(height == 0);
}

static void
test_scale_then_constrain(void)
{
	monitor_scale_factor factor    = {5};
	icccm_size_hints     hints     = {
		.flags	    = PMinSize,
		.min_width  = 8,
		.min_height = 8,
	};
	int		     converted = 0;
	int		     width;
	int		     height;
	icccm_constrain_status status;

	assert(monitor_scale_apply(7, factor, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 9);

	width  = converted;
	height = converted;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 9);
	assert(height == 9);
}

static void
test_repeated_constrain_from_original_source(void)
{
	monitor_scale_factor factor = {5};
	icccm_size_hints     hints  = {
		.flags	    = PMinSize,
		.min_width  = 8,
		.min_height = 8,
	};
	int iteration;

	for (iteration = 0; iteration < 1000; iteration++) {
		int		     converted = 0;
		int		     width;
		int		     height;
		icccm_constrain_status status;

		assert(monitor_scale_apply(7, factor, &converted)
		       == MONITOR_SCALE_CONVERSION_OK);
		width  = converted;
		height = converted;
		status = icccm_constrain_size(&hints, &width, &height, NULL);
		assert(status == ICCCM_CONSTRAIN_OK);
		assert(width == 9);
		assert(height == 9);
	}
}

int
main(void)
{
	test_min_max_clamp();
	test_base_inc_grid();
	test_aspect_with_pbase_subtracts_base();
	test_aspect_without_pbase_does_not_use_min();
	test_base_greater_than_min_skips_aspect_base();
	test_fixed_size();
	test_gravity_northwest_default();
	test_zero_increment_invalid();
	test_aspect_overflow_initializes_outputs();
	test_scale_then_constrain();
	test_repeated_constrain_from_original_source();

	return 0;
}
