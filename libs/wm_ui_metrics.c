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

#include "libs/wm_ui_metrics.h"

monitor_scale_conversion_status
wm_ui_scale_px(int source, monitor_scale_factor factor, int *px)
{
	return monitor_scale_apply(source, factor, px);
}

bool
wm_ui_cache_key_equal(const wm_ui_cache_key *a, const wm_ui_cache_key *b)
{
	if (a == NULL || b == NULL) {
		return false;
	}

	return a->scale_quarters == b->scale_quarters
	       && a->target_width == b->target_width
	       && a->target_height == b->target_height
	       && a->generation == b->generation;
}

wm_ui_resource_generation
wm_ui_resource_generation_current(void)
{
	wm_ui_resource_generation current = {
	    .generation = WM_UI_RESOURCE_GENERATION_INITIAL,
	};

	return current;
}
