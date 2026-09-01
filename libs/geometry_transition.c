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

static void
zero_dest_frame_size(rectangle *dest_frame)
{
	if (dest_frame == NULL) {
		return;
	}
	dest_frame->width  = 0;
	dest_frame->height = 0;
}

geometry_transition_status
geometry_transition_plan(geometry_transition_kind kind,
    size_rect logical_client, size_rect src_client, bool eligible,
    monitor_scale_factor dest_factor, const size_borders *dest_chrome,
    const icccm_size_hints *hints, rectangle *dest_frame)
{
	int dest_width	= 0;
	int dest_height = 0;

	(void)kind;
	(void)src_client;
	(void)eligible;
	(void)dest_chrome;
	(void)hints;

	if (dest_frame == NULL
	    || dest_factor.quarters < MONITOR_SCALE_QUARTERS_MIN
	    || dest_factor.quarters > MONITOR_SCALE_QUARTERS_MAX) {
		zero_dest_frame_size(dest_frame);
		return GEOMETRY_TRANSITION_INVALID_ARGUMENT;
	}

	if (monitor_scale_apply(logical_client.width, dest_factor,
		&dest_width)
	    != MONITOR_SCALE_CONVERSION_OK) {
		zero_dest_frame_size(dest_frame);
		return GEOMETRY_TRANSITION_OVERFLOW;
	}
	if (monitor_scale_apply(logical_client.height, dest_factor,
		&dest_height)
	    != MONITOR_SCALE_CONVERSION_OK) {
		zero_dest_frame_size(dest_frame);
		return GEOMETRY_TRANSITION_OVERFLOW;
	}

	dest_frame->width  = dest_width;
	dest_frame->height = dest_height;

	return GEOMETRY_TRANSITION_OK;
}
