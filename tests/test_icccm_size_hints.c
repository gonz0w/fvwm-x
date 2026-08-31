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

#include <assert.h>

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

int
main(void)
{
	test_min_max_clamp();

	return 0;
}
