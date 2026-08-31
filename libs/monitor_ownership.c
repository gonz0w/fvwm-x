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

#include <stdbool.h>

typedef __int128 monitor_ownership_wide;

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

static monitor_ownership_wide
overlap_area(
    const rectangle *window, const monitor_topology_monitor *monitor)
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

	win_right  = (monitor_ownership_wide)window->x + window->width;
	win_bottom = (monitor_ownership_wide)window->y + window->height;
	mon_right  = (monitor_ownership_wide)monitor->x + monitor->width;
	mon_bottom = (monitor_ownership_wide)monitor->y + monitor->height;
	left	   = window->x > monitor->x ? window->x : monitor->x;
	top	   = window->y > monitor->y ? window->y : monitor->y;
	right	   = win_right < mon_right ? win_right : mon_right;
	bottom	   = win_bottom < mon_bottom ? win_bottom : mon_bottom;
	if (right <= left || bottom <= top) {
		return 0;
	}
	width  = right - left;
	height = bottom - top;

	return width * height;
}

monitor_ownership_status
monitor_ownership_resolve(const rectangle *window,
    const monitor_topology_monitor *monitors, int monitor_count,
    Atom incumbent_atom, int hysteresis_px, monitor_ownership_result *result)
{
	int			 i;
	int			 best_index;
	monitor_ownership_wide	 best_area;
	bool			 unique;

	(void)incumbent_atom;
	(void)hysteresis_px;
	if (result == NULL) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}
	*result = (monitor_ownership_result)MONITOR_OWNERSHIP_RESULT_SAFE_INIT;
	if (window == NULL || monitors == NULL || monitor_count <= 0) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}

	best_index = -1;
	best_area  = 0;
	unique	   = false;
	for (i = 0; i < monitor_count; i++) {
		monitor_ownership_wide area;

		area = overlap_area(window, &monitors[i]);
		if (area <= 0) {
			continue;
		}
		if (best_index < 0 || area > best_area) {
			best_index = i;
			best_area  = area;
			unique	   = true;
		} else if (area == best_area) {
			unique = false;
		}
	}
	if (best_index < 0 || !unique) {
		return MONITOR_OWNERSHIP_OK;
	}

	result->name_atom     = monitors[best_index].name_atom;
	result->name	      = monitors[best_index].name;
	result->extra_overlap = 0;
	result->reason	      = window_is_contained(window,
				    &monitors[best_index])
				    ? MONITOR_OWNERSHIP_REASON_CONTAINMENT
				    : MONITOR_OWNERSHIP_REASON_OVERLAP;

	return MONITOR_OWNERSHIP_OK;
}
