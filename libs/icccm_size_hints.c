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
#include "libs/gravity.h"

#include <limits.h>
#include <stdbool.h>
#include <X11/Xutil.h>

typedef __int128 icccm_wide;

static void
zero_outputs(int *width, int *height)
{
	if (width != NULL) {
		*width = 0;
	}
	if (height != NULL) {
		*height = 0;
	}
}

static bool
wide_fits_int(icccm_wide value)
{
	return value >= INT_MIN && value <= INT_MAX;
}

static void
clamp_min_max(const icccm_size_hints *hints, int *width, int *height)
{
	if ((hints->flags & PMinSize) != 0) {
		if (*width < hints->min_width) {
			*width = hints->min_width;
		}
		if (*height < hints->min_height) {
			*height = hints->min_height;
		}
	}
	if ((hints->flags & PMaxSize) != 0) {
		if (*width > hints->max_width) {
			*width = hints->max_width;
		}
		if (*height > hints->max_height) {
			*height = hints->max_height;
		}
	}
}

static int
grid_origin(const icccm_size_hints *hints, int base, int min)
{
	if ((hints->flags & PBaseSize) != 0) {
		return base;
	}
	if ((hints->flags & PMinSize) != 0) {
		return min;
	}

	return 0;
}

static icccm_constrain_status
snap_to_grid(int *value, int base, int inc)
{
	icccm_wide delta;
	icccm_wide magnitude;
	icccm_wide quotient;
	icccm_wide remainder;
	icccm_wide stepped;
	icccm_wide snapped;
	bool	   negative;

	delta	  = (icccm_wide)*value - base;
	negative  = delta < 0;
	magnitude = negative ? -delta : delta;
	quotient  = magnitude / inc;
	remainder = magnitude % inc;
	if (2 * remainder >= (icccm_wide)inc) {
		quotient++;
	}
	stepped = negative ? -quotient : quotient;
	snapped = (icccm_wide)base + stepped * inc;
	if (!wide_fits_int(snapped)) {
		return ICCCM_CONSTRAIN_OVERFLOW;
	}
	*value = (int)snapped;

	return ICCCM_CONSTRAIN_OK;
}

static bool
aspect_subtracts_base(const icccm_size_hints *hints)
{
	if ((hints->flags & PBaseSize) == 0) {
		return false;
	}
	if ((hints->flags & PMinSize) != 0
	    && (hints->base_width > hints->min_width
		|| hints->base_height > hints->min_height)) {
		return false;
	}

	return true;
}

static bool
within_min(const icccm_size_hints *hints, int value, int min)
{
	return (hints->flags & PMinSize) == 0 || value >= min;
}

static bool
within_max(const icccm_size_hints *hints, int value, int max)
{
	return (hints->flags & PMaxSize) == 0 || value <= max;
}

static icccm_constrain_status
adjust_aspect(const icccm_size_hints *hints, int *width, int *height)
{
	int	   base_w = 0;
	int	   base_h = 0;
	icccm_wide aw;
	icccm_wide ah;

	if (aspect_subtracts_base(hints)) {
		base_w = hints->base_width;
		base_h = hints->base_height;
	}
	aw = (icccm_wide)*width - base_w;
	ah = (icccm_wide)*height - base_h;
	if (aw <= 0 || ah <= 0) {
		return ICCCM_CONSTRAIN_OK;
	}

	if (hints->min_aspect.x > 0 && hints->min_aspect.y > 0
	    && aw * hints->min_aspect.y < ah * hints->min_aspect.x) {
		icccm_wide min_x  = hints->min_aspect.x;
		icccm_wide min_y  = hints->min_aspect.y;
		icccm_wide ah_new = (aw * min_y) / min_x;
		icccm_wide new_h  = ah_new + base_h;

		if (wide_fits_int(new_h) && (int)new_h >= 1
		    && within_min(hints, (int)new_h, hints->min_height)) {
			*height = (int)new_h;
			ah	= ah_new;
		} else {
			icccm_wide aw_new =
			    (ah * min_x + min_y - 1) / min_y;
			icccm_wide new_w = aw_new + base_w;

			if (!wide_fits_int(new_w)
			    || !within_max(
				hints, (int)new_w, hints->max_width)) {
				return ICCCM_CONSTRAIN_OVERFLOW;
			}
			*width = (int)new_w;
			aw     = aw_new;
		}
	}

	if (hints->max_aspect.x > 0 && hints->max_aspect.y > 0
	    && aw * hints->max_aspect.y > ah * hints->max_aspect.x) {
		icccm_wide max_x  = hints->max_aspect.x;
		icccm_wide max_y  = hints->max_aspect.y;
		icccm_wide aw_new = (ah * max_x) / max_y;
		icccm_wide new_w  = aw_new + base_w;

		if (wide_fits_int(new_w) && (int)new_w >= 1
		    && within_min(hints, (int)new_w, hints->min_width)) {
			*width = (int)new_w;
		} else {
			icccm_wide ah_new =
			    (aw * max_y + max_x - 1) / max_x;
			icccm_wide new_h = ah_new + base_h;

			if (!wide_fits_int(new_h)
			    || !within_max(
				hints, (int)new_h, hints->max_height)) {
				return ICCCM_CONSTRAIN_OVERFLOW;
			}
			*height = (int)new_h;
		}
	}

	return ICCCM_CONSTRAIN_OK;
}

icccm_constrain_status
icccm_constrain_size(const icccm_size_hints *hints, int *width, int *height,
    rectangle *frame)
{
	int		       constrained_width;
	int		       constrained_height;
	int		       original_width;
	int		       original_height;
	icccm_constrain_status status;

	if (hints == NULL || width == NULL || height == NULL) {
		zero_outputs(width, height);
		return ICCCM_CONSTRAIN_INVALID_ARGUMENT;
	}

	if ((hints->flags & PResizeInc) != 0
	    && (hints->width_inc == 0 || hints->height_inc == 0)) {
		zero_outputs(width, height);
		return ICCCM_CONSTRAIN_INVALID_ARGUMENT;
	}

	original_width	   = *width;
	original_height	   = *height;
	constrained_width  = *width;
	constrained_height = *height;

	clamp_min_max(hints, &constrained_width, &constrained_height);

	if ((hints->flags & PResizeInc) != 0) {
		int grid_width = grid_origin(
		    hints, hints->base_width, hints->min_width);
		int grid_height = grid_origin(
		    hints, hints->base_height, hints->min_height);

		status = snap_to_grid(
		    &constrained_width, grid_width, hints->width_inc);
		if (status != ICCCM_CONSTRAIN_OK) {
			zero_outputs(width, height);
			return status;
		}
		status = snap_to_grid(
		    &constrained_height, grid_height, hints->height_inc);
		if (status != ICCCM_CONSTRAIN_OK) {
			zero_outputs(width, height);
			return status;
		}
		clamp_min_max(hints, &constrained_width, &constrained_height);
	}

	if ((hints->flags & PAspect) != 0) {
		status = adjust_aspect(
		    hints, &constrained_width, &constrained_height);
		if (status != ICCCM_CONSTRAIN_OK) {
			zero_outputs(width, height);
			return status;
		}
		clamp_min_max(hints, &constrained_width, &constrained_height);
	}

	*width	= constrained_width;
	*height = constrained_height;
	if (frame != NULL) {
		int gravity = (hints->flags & PWinGravity) != 0 ?
		    hints->win_gravity :
		    NorthWestGravity;

		gravity_resize(gravity, frame,
		    constrained_width - original_width,
		    constrained_height - original_height);
	}

	return ICCCM_CONSTRAIN_OK;
}
