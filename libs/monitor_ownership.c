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

#include "libs/monitor_ownership.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

typedef __int128 monitor_ownership_wide;

static int
identity_cmp(const monitor_topology_monitor *left,
    const monitor_topology_monitor *right)
{
	const char *left_name;
	const char *right_name;

	if (left->name_atom < right->name_atom) {
		return -1;
	}
	if (left->name_atom > right->name_atom) {
		return 1;
	}
	left_name  = left->name != NULL ? left->name : "";
	right_name = right->name != NULL ? right->name : "";

	return strcmp(left_name, right_name);
}

static bool
window_is_contained(const rectangle *window,
    const monitor_topology_monitor *monitor)
{
	monitor_ownership_wide win_right;
	monitor_ownership_wide win_bottom;
	monitor_ownership_wide mon_right;
	monitor_ownership_wide mon_bottom;

	if (window->width <= 0 || window->height <= 0 || monitor->width <= 0
	    || monitor->height <= 0) {
		return false;
	}
	if (window->x < monitor->x || window->y < monitor->y) {
		return false;
	}
	win_right  = (monitor_ownership_wide)window->x + window->width;
	win_bottom = (monitor_ownership_wide)window->y + window->height;
	mon_right  = (monitor_ownership_wide)monitor->x + monitor->width;
	mon_bottom = (monitor_ownership_wide)monitor->y + monitor->height;

	return win_right <= mon_right && win_bottom <= mon_bottom;
}

static monitor_ownership_status
overlap_area(const rectangle *window,
    const monitor_topology_monitor *monitor, monitor_ownership_wide *area)
{
	monitor_ownership_wide left;
	monitor_ownership_wide top;
	monitor_ownership_wide right;
	monitor_ownership_wide bottom;
	monitor_ownership_wide win_right;
	monitor_ownership_wide win_bottom;
	monitor_ownership_wide mon_right;
	monitor_ownership_wide mon_bottom;
	monitor_ownership_wide width;
	monitor_ownership_wide height;
	monitor_ownership_wide product;

	win_right  = (monitor_ownership_wide)window->x + window->width;
	win_bottom = (monitor_ownership_wide)window->y + window->height;
	mon_right  = (monitor_ownership_wide)monitor->x + monitor->width;
	mon_bottom = (monitor_ownership_wide)monitor->y + monitor->height;
	left	   = window->x > monitor->x ? window->x : monitor->x;
	top	   = window->y > monitor->y ? window->y : monitor->y;
	right	   = win_right < mon_right ? win_right : mon_right;
	bottom	   = win_bottom < mon_bottom ? win_bottom : mon_bottom;
	if (right <= left || bottom <= top) {
		*area = 0;
		return MONITOR_OWNERSHIP_OK;
	}
	width	= right - left;
	height	= bottom - top;
	product = width * height;
	if (product > INT_MAX) {
		*area = 0;
		return MONITOR_OWNERSHIP_OVERFLOW;
	}
	*area = product;

	return MONITOR_OWNERSHIP_OK;
}

static monitor_ownership_wide
gap_distance(
    const rectangle *window, const monitor_topology_monitor *monitor)
{
	monitor_ownership_wide left	= monitor->x;
	monitor_ownership_wide top	= monitor->y;
	monitor_ownership_wide right	= left + monitor->width;
	monitor_ownership_wide bottom	= top + monitor->height;
	monitor_ownership_wide point_x	= window->x;
	monitor_ownership_wide point_y	= window->y;
	monitor_ownership_wide distance = 0;

	if (point_x < left) {
		distance += left - point_x;
	} else if (point_x > right) {
		distance += point_x - right;
	}
	if (point_y < top) {
		distance += top - point_y;
	} else if (point_y > bottom) {
		distance += point_y - bottom;
	}

	return distance;
}

static int
saturate_extra_overlap(monitor_ownership_wide extra)
{
	if (extra > INT_MAX) {
		return INT_MAX;
	}
	if (extra < INT_MIN) {
		return INT_MIN;
	}

	return (int)extra;
}

static void
assign_owner(monitor_ownership_result *result,
    const monitor_topology_monitor *monitor, monitor_ownership_reason reason,
    int extra_overlap)
{
	result->name_atom     = monitor->name_atom;
	result->name	      = monitor->name;
	result->reason	      = reason;
	result->extra_overlap = extra_overlap;
}

monitor_ownership_status
monitor_ownership_resolve(const rectangle *window,
    const monitor_topology_monitor *monitors, int monitor_count,
    Atom incumbent_atom, int hysteresis_px, monitor_ownership_result *result)
{
	int			 i;
	int			 best_index;
	int			 incumbent_index;
	int			 hysteresis;
	int			 extra_overlap;
	monitor_ownership_wide	 best_area;
	monitor_ownership_wide	 incumbent_area;
	monitor_ownership_wide	 best_distance;
	monitor_ownership_wide	 extra;
	monitor_ownership_reason reason;

	if (result == NULL) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}
	*result = (monitor_ownership_result)MONITOR_OWNERSHIP_RESULT_SAFE_INIT;
	if (window == NULL || monitor_count < 0) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}
	if (monitor_count == 0) {
		result->reason = MONITOR_OWNERSHIP_REASON_FALLBACK;
		return MONITOR_OWNERSHIP_OK;
	}
	if (monitors == NULL) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}

	hysteresis	= hysteresis_px < 0 ? 0 : hysteresis_px;
	best_index	= -1;
	best_area	= 0;
	incumbent_index = -1;
	incumbent_area	= 0;
	for (i = 0; i < monitor_count; i++) {
		monitor_ownership_wide	 area;
		monitor_ownership_status area_status;

		area_status = overlap_area(window, &monitors[i], &area);
		if (area_status != MONITOR_OWNERSHIP_OK) {
			*result = (monitor_ownership_result)
			    MONITOR_OWNERSHIP_RESULT_SAFE_INIT;
			return area_status;
		}
		if (area <= 0) {
			continue;
		}
		if (incumbent_atom != None
		    && monitors[i].name_atom == incumbent_atom) {
			incumbent_index = i;
			incumbent_area	= area;
		}
		if (best_index < 0 || area > best_area
		    || (area == best_area
			&& identity_cmp(&monitors[i], &monitors[best_index])
			       < 0)) {
			best_index = i;
			best_area  = area;
		}
	}
	if (best_index >= 0) {
		reason = window_is_contained(window, &monitors[best_index]) ?
		    MONITOR_OWNERSHIP_REASON_CONTAINMENT :
		    MONITOR_OWNERSHIP_REASON_OVERLAP;
		if (incumbent_index >= 0) {
			extra	      = best_area - incumbent_area;
			extra_overlap = saturate_extra_overlap(extra);
			if (extra <= hysteresis) {
				assign_owner(result,
				    &monitors[incumbent_index],
				    MONITOR_OWNERSHIP_REASON_INCUMBENT,
				    extra_overlap);
				return MONITOR_OWNERSHIP_OK;
			}
			assign_owner(result, &monitors[best_index], reason,
			    extra_overlap);
			return MONITOR_OWNERSHIP_OK;
		}
		assign_owner(result, &monitors[best_index], reason, 0);
		return MONITOR_OWNERSHIP_OK;
	}

	best_index    = 0;
	best_distance = gap_distance(window, &monitors[0]);
	for (i = 1; i < monitor_count; i++) {
		monitor_ownership_wide distance;

		distance = gap_distance(window, &monitors[i]);
		if (distance < best_distance
		    || (distance == best_distance
			&& identity_cmp(&monitors[i], &monitors[best_index])
			       < 0)) {
			best_index    = i;
			best_distance = distance;
		}
	}
	assign_owner(
	    result, &monitors[best_index], MONITOR_OWNERSHIP_REASON_GAP, 0);

	return MONITOR_OWNERSHIP_OK;
}

static void
zero_work_rect(rectangle *out)
{
	if (out == NULL) {
		return;
	}
	out->x	    = 0;
	out->y	    = 0;
	out->width  = 0;
	out->height = 0;
}

monitor_ownership_status
monitor_ownership_work_rect(const rectangle *monitor_rect, int strut_left,
    int strut_right, int strut_top, int strut_bottom, rectangle *out)
{
	monitor_ownership_wide x;
	monitor_ownership_wide y;
	monitor_ownership_wide width;
	monitor_ownership_wide height;

	if (out == NULL) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}
	zero_work_rect(out);
	if (monitor_rect == NULL || strut_left < 0 || strut_right < 0
	    || strut_top < 0 || strut_bottom < 0 || monitor_rect->width < 0
	    || monitor_rect->height < 0) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}

	x      = (monitor_ownership_wide)monitor_rect->x + strut_left;
	y      = (monitor_ownership_wide)monitor_rect->y + strut_top;
	width  = (monitor_ownership_wide)monitor_rect->width - strut_left
	    - strut_right;
	height = (monitor_ownership_wide)monitor_rect->height - strut_top
	    - strut_bottom;
	if (width < 0 || height < 0) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}
	if (x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX
	    || width > INT_MAX || height > INT_MAX) {
		return MONITOR_OWNERSHIP_OVERFLOW;
	}

	out->x	    = (int)x;
	out->y	    = (int)y;
	out->width  = (int)width;
	out->height = (int)height;

	return MONITOR_OWNERSHIP_OK;
}

static void
zero_boundingbox(boundingbox *out)
{
	if (out == NULL) {
		return;
	}
	out->left   = 0;
	out->right  = 0;
	out->top    = 0;
	out->bottom = 0;
}

monitor_ownership_status
monitor_ownership_work_insets(const rectangle *monitor_rect,
    const rectangle *work_rect, boundingbox *out)
{
	monitor_ownership_wide left;
	monitor_ownership_wide right;
	monitor_ownership_wide top;
	monitor_ownership_wide bottom;
	monitor_ownership_wide mon_right;
	monitor_ownership_wide mon_bottom;
	monitor_ownership_wide work_right;
	monitor_ownership_wide work_bottom;

	if (out == NULL) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}
	zero_boundingbox(out);
	if (monitor_rect == NULL || work_rect == NULL) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}

	left	    = (monitor_ownership_wide)work_rect->x - monitor_rect->x;
	top	    = (monitor_ownership_wide)work_rect->y - monitor_rect->y;
	mon_right   = (monitor_ownership_wide)monitor_rect->x
	    + monitor_rect->width;
	mon_bottom  = (monitor_ownership_wide)monitor_rect->y
	    + monitor_rect->height;
	work_right  = (monitor_ownership_wide)work_rect->x + work_rect->width;
	work_bottom = (monitor_ownership_wide)work_rect->y + work_rect->height;
	right	    = mon_right - work_right;
	bottom	    = mon_bottom - work_bottom;
	if (left < INT_MIN || left > INT_MAX || right < INT_MIN
	    || right > INT_MAX || top < INT_MIN || top > INT_MAX
	    || bottom < INT_MIN || bottom > INT_MAX) {
		return MONITOR_OWNERSHIP_OVERFLOW;
	}

	out->left   = (int)left;
	out->right  = (int)right;
	out->top    = (int)top;
	out->bottom = (int)bottom;

	return MONITOR_OWNERSHIP_OK;
}

monitor_ownership_status
monitor_ownership_work_rect_paged(const rectangle *work_rect, int page_x,
    int page_y, rectangle *out)
{
	monitor_ownership_wide x;
	monitor_ownership_wide y;

	if (out == NULL) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}
	zero_work_rect(out);
	if (work_rect == NULL) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}

	x = (monitor_ownership_wide)work_rect->x + page_x;
	y = (monitor_ownership_wide)work_rect->y + page_y;
	if (x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX) {
		return MONITOR_OWNERSHIP_OVERFLOW;
	}

	out->x	    = (int)x;
	out->y	    = (int)y;
	out->width  = work_rect->width;
	out->height = work_rect->height;

	return MONITOR_OWNERSHIP_OK;
}
