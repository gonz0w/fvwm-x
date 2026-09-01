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
#include "libs/gravity.h"

#include <limits.h>

static void
zero_dest_frame_size(rectangle *dest_frame)
{
	if (dest_frame == NULL) {
		return;
	}
	dest_frame->width  = 0;
	dest_frame->height = 0;
}

static geometry_transition_status
fail_invalid(rectangle *dest_frame)
{
	zero_dest_frame_size(dest_frame);
	return GEOMETRY_TRANSITION_INVALID_ARGUMENT;
}

static geometry_transition_status
fail_overflow(rectangle *dest_frame)
{
	zero_dest_frame_size(dest_frame);
	return GEOMETRY_TRANSITION_OVERFLOW;
}

static bool
add_checked(int a, int b, int *out)
{
	if (b > 0 && a > INT_MAX - b) {
		return false;
	}
	if (b < 0 && a < INT_MIN - b) {
		return false;
	}
	*out = a + b;
	return true;
}

static bool
sub_nonneg(int a, int b, int *out)
{
	if (b < 0 || a < b) {
		return false;
	}
	*out = a - b;
	return true;
}

static bool
factor_is_valid(monitor_scale_factor factor)
{
	return factor.quarters >= MONITOR_SCALE_QUARTERS_MIN
	    && factor.quarters <= MONITOR_SCALE_QUARTERS_MAX;
}

static geometry_transition_status
apply_logical_axis(int logical, monitor_scale_factor dest_factor, int *dest)
{
	monitor_scale_conversion_status status;

	status = monitor_scale_apply(logical, dest_factor, dest);
	if (status == MONITOR_SCALE_CONVERSION_OVERFLOW) {
		return GEOMETRY_TRANSITION_OVERFLOW;
	}
	if (status != MONITOR_SCALE_CONVERSION_OK) {
		return GEOMETRY_TRANSITION_INVALID_ARGUMENT;
	}

	return GEOMETRY_TRANSITION_OK;
}

static geometry_transition_status
apply_logical_client(size_rect logical_client,
    monitor_scale_factor dest_factor, int *dest_width, int *dest_height,
    rectangle *dest_frame)
{
	geometry_transition_status status;

	status = apply_logical_axis(
	    logical_client.width, dest_factor, dest_width);
	if (status == GEOMETRY_TRANSITION_OVERFLOW) {
		return fail_overflow(dest_frame);
	}
	if (status != GEOMETRY_TRANSITION_OK) {
		return fail_invalid(dest_frame);
	}
	status = apply_logical_axis(
	    logical_client.height, dest_factor, dest_height);
	if (status == GEOMETRY_TRANSITION_OVERFLOW) {
		return fail_overflow(dest_frame);
	}
	if (status != GEOMETRY_TRANSITION_OK) {
		return fail_invalid(dest_frame);
	}

	return GEOMETRY_TRANSITION_OK;
}

static geometry_transition_status
constrain_dest_client(const icccm_size_hints *hints, int *dest_width,
    int *dest_height, rectangle *dest_frame)
{
	icccm_constrain_status constrain_status;

	if (hints == NULL) {
		return GEOMETRY_TRANSITION_OK;
	}
	constrain_status = icccm_constrain_size(
	    hints, dest_width, dest_height, NULL);
	if (constrain_status == ICCCM_CONSTRAIN_OVERFLOW) {
		return fail_overflow(dest_frame);
	}
	if (constrain_status != ICCCM_CONSTRAIN_OK) {
		return fail_invalid(dest_frame);
	}

	return GEOMETRY_TRANSITION_OK;
}

static geometry_transition_status
compose_dest_frame(int dest_width, int dest_height,
    const size_borders *dest_chrome, const icccm_size_hints *hints,
    rectangle *dest_frame, geometry_transition_status ok_status)
{
	int frame_width;
	int frame_height;
	int wdiff;
	int hdiff;

	if (!add_checked(dest_width, dest_chrome->total_size.width,
		&frame_width)
	    || !add_checked(dest_height, dest_chrome->total_size.height,
		   &frame_height)) {
		return fail_overflow(dest_frame);
	}

	if (dest_frame->width == frame_width
	    && dest_frame->height == frame_height) {
		if (ok_status == GEOMETRY_TRANSITION_OK) {
			return GEOMETRY_TRANSITION_NOOP;
		}
		return ok_status;
	}

	wdiff = frame_width - dest_frame->width;
	hdiff = frame_height - dest_frame->height;
	if (hints != NULL && (hints->flags & PWinGravity) != 0) {
		gravity_resize(hints->win_gravity, dest_frame, wdiff, hdiff);
	} else {
		dest_frame->width  = frame_width;
		dest_frame->height = frame_height;
	}

	return ok_status;
}

static geometry_transition_status
plan_maximized(const size_borders *dest_chrome, const icccm_size_hints *hints,
    rectangle *dest_frame)
{
	int			 dest_width;
	int			 dest_height;
	geometry_transition_status status;

	if (!sub_nonneg(dest_frame->width, dest_chrome->total_size.width,
		&dest_width)
	    || !sub_nonneg(dest_frame->height,
		   dest_chrome->total_size.height, &dest_height)) {
		return fail_invalid(dest_frame);
	}
	status = constrain_dest_client(
	    hints, &dest_width, &dest_height, dest_frame);
	if (status != GEOMETRY_TRANSITION_OK) {
		return status;
	}

	return compose_dest_frame(dest_width, dest_height, dest_chrome, NULL,
	    dest_frame, GEOMETRY_TRANSITION_OK);
}

static geometry_transition_status
plan_from_logical(size_rect logical_client, size_rect src_client,
    bool eligible, monitor_scale_factor dest_factor,
    const size_borders *dest_chrome, const icccm_size_hints *hints,
    rectangle *dest_frame)
{
	int			 dest_width;
	int			 dest_height;
	geometry_transition_status status;

	if (!eligible) {
		return compose_dest_frame(src_client.width, src_client.height,
		    dest_chrome, NULL, dest_frame,
		    GEOMETRY_TRANSITION_INELIGIBLE);
	}

	status = apply_logical_client(logical_client, dest_factor,
	    &dest_width, &dest_height, dest_frame);
	if (status != GEOMETRY_TRANSITION_OK) {
		return status;
	}
	status = constrain_dest_client(
	    hints, &dest_width, &dest_height, dest_frame);
	if (status != GEOMETRY_TRANSITION_OK) {
		return status;
	}

	return compose_dest_frame(dest_width, dest_height, dest_chrome, hints,
	    dest_frame, GEOMETRY_TRANSITION_OK);
}

geometry_transition_status
geometry_transition_plan(geometry_transition_kind kind,
    size_rect logical_client, size_rect src_client, bool eligible,
    monitor_scale_factor dest_factor, const size_borders *dest_chrome,
    const icccm_size_hints *hints, rectangle *dest_frame)
{
	if (dest_frame == NULL || !factor_is_valid(dest_factor)) {
		return fail_invalid(dest_frame);
	}
	if (kind != GEOMETRY_TRANSITION_FULLSCREEN && dest_chrome == NULL) {
		return fail_invalid(dest_frame);
	}

	switch (kind) {
	case GEOMETRY_TRANSITION_FULLSCREEN:
		return GEOMETRY_TRANSITION_OK;
	case GEOMETRY_TRANSITION_MAXIMIZED:
		return plan_maximized(dest_chrome, hints, dest_frame);
	case GEOMETRY_TRANSITION_NORMAL:
	case GEOMETRY_TRANSITION_RESTORE:
		return plan_from_logical(logical_client, src_client, eligible,
		    dest_factor, dest_chrome, hints, dest_frame);
	default:
		return fail_invalid(dest_frame);
	}
}
