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

#include <X11/Xutil.h>

icccm_constrain_status
icccm_constrain_size(const icccm_size_hints *hints, int *width, int *height,
    rectangle *frame)
{
	int constrained_width;
	int constrained_height;

	(void)frame;

	if (hints == NULL || width == NULL || height == NULL) {
		if (width != NULL) {
			*width = 0;
		}
		if (height != NULL) {
			*height = 0;
		}
		return ICCCM_CONSTRAIN_INVALID_ARGUMENT;
	}

	constrained_width  = *width;
	constrained_height = *height;

	if ((hints->flags & PMinSize) != 0) {
		if (constrained_width < hints->min_width) {
			constrained_width = hints->min_width;
		}
		if (constrained_height < hints->min_height) {
			constrained_height = hints->min_height;
		}
	}
	if ((hints->flags & PMaxSize) != 0) {
		if (constrained_width > hints->max_width) {
			constrained_width = hints->max_width;
		}
		if (constrained_height > hints->max_height) {
			constrained_height = hints->max_height;
		}
	}

	*width	= constrained_width;
	*height = constrained_height;

	return ICCCM_CONSTRAIN_OK;
}
