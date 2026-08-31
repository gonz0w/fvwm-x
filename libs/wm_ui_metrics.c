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

#include <limits.h>

static void
zero_title_outputs(int *thickness, int *offset)
{
	if (thickness != NULL) {
		*thickness = 0;
	}
	if (offset != NULL) {
		*offset = 0;
	}
}

static monitor_scale_conversion_status
apply_title_source(int source, monitor_scale_factor factor,
    int scaled_font_height, int *composed)
{
	monitor_scale_conversion_status status;
	int scaled = 0;

	if (source <= 0) {
		return MONITOR_SCALE_CONVERSION_OK;
	}
	status = wm_ui_scale_px(source, factor, &scaled);
	if (status != MONITOR_SCALE_CONVERSION_OK) {
		return status;
	}
	if (scaled > *composed) {
		*composed = scaled;
	} else if (scaled < scaled_font_height) {
		*composed = scaled_font_height;
	}

	return MONITOR_SCALE_CONVERSION_OK;
}

monitor_scale_conversion_status
wm_ui_scale_px(int source, monitor_scale_factor factor, int *px)
{
	return monitor_scale_apply(source, factor, px);
}

monitor_scale_conversion_status
wm_ui_compose_title_thickness(int scaled_font_height, int extra_source,
    int explicit_title_source, int min_title_source,
    monitor_scale_factor factor, int *thickness, int *offset)
{
	monitor_scale_conversion_status status;
	int extra_px = 0;
	int composed;
	int surplus;

	zero_title_outputs(thickness, offset);
	if (thickness == NULL || offset == NULL || scaled_font_height < 0) {
		return MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT;
	}

	status = wm_ui_scale_px(extra_source, factor, &extra_px);
	if (status != MONITOR_SCALE_CONVERSION_OK) {
		return status;
	}
	if (extra_px < 0 || scaled_font_height > INT_MAX - extra_px) {
		return MONITOR_SCALE_CONVERSION_OVERFLOW;
	}
	composed = scaled_font_height + extra_px;

	status = apply_title_source(explicit_title_source, factor,
	    scaled_font_height, &composed);
	if (status != MONITOR_SCALE_CONVERSION_OK) {
		zero_title_outputs(thickness, offset);
		return status;
	}
	status = apply_title_source(
	    min_title_source, factor, scaled_font_height, &composed);
	if (status != MONITOR_SCALE_CONVERSION_OK) {
		zero_title_outputs(thickness, offset);
		return status;
	}

	surplus = composed - scaled_font_height - extra_px;
	if (surplus > 0) {
		*offset = surplus / 2;
	}
	*thickness = composed;

	return MONITOR_SCALE_CONVERSION_OK;
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
