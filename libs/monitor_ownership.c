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

static void
assign_owner(monitor_ownership_result *result,
    const monitor_topology_monitor *monitor, monitor_ownership_reason reason)
{
	result->name_atom     = monitor->name_atom;
	result->name	      = monitor->name;
	result->reason	      = reason;
	result->extra_overlap = 0;
}

monitor_ownership_status
monitor_ownership_resolve(const rectangle *window,
    const monitor_topology_monitor *monitors, int monitor_count,
    Atom incumbent_atom, int hysteresis_px, monitor_ownership_result *result)
{
	int			 i;
	int			 best_index;
	monitor_ownership_wide	 best_area;
	monitor_ownership_wide	 best_distance;

	(void)incumbent_atom;
	(void)hysteresis_px;
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

	best_index = -1;
	best_area  = 0;
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
		if (best_index < 0 || area > best_area
		    || (area == best_area
			&& identity_cmp(&monitors[i], &monitors[best_index])
			       < 0)) {
			best_index = i;
			best_area  = area;
		}
	}
	if (best_index >= 0) {
		assign_owner(result, &monitors[best_index],
		    window_is_contained(window, &monitors[best_index])
			? MONITOR_OWNERSHIP_REASON_CONTAINMENT
			: MONITOR_OWNERSHIP_REASON_OVERLAP);
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
	    result, &monitors[best_index], MONITOR_OWNERSHIP_REASON_GAP);

	return MONITOR_OWNERSHIP_OK;
}
