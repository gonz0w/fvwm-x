/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "form_scale.h"
#include "libs/monitor_scale.h"

#include <limits.h>
#include <stdint.h>

bool form_scale_extent(int logical, int required, unsigned int quarters,
	int *extent)
{
	int scaled;

	*extent = 0;
	if (logical < 0 || required < 0 || required > 32767 ||
	    monitor_scale_apply(logical, (monitor_scale_factor){quarters},
		&scaled) != MONITOR_SCALE_CONVERSION_OK || scaled > 32767)
		return false;
	*extent = scaled > required ? scaled : required;
	return true;
}

bool form_scale_metrics_prepare(unsigned int quarters, int logical_pad,
	form_scale_metrics *metrics)
{
	form_scale_metrics candidate;

	if (!form_scale_extent(3, 1, quarters, &candidate.text) ||
	    !form_scale_extent(3, 1, quarters, &candidate.box) ||
	    !form_scale_extent(10, 1, quarters, &candidate.horizontal) ||
	    !form_scale_extent(5, 1, quarters, &candidate.vertical) ||
	    !form_scale_extent(16, 1, quarters, &candidate.gutter) ||
	    !form_scale_extent(10, 1, quarters, &candidate.thumb_width) ||
	    !form_scale_extent(2, 1, quarters, &candidate.thumb_inset) ||
	    !form_scale_extent(4, 1, quarters, &candidate.thumb_min) ||
	    !form_scale_extent(logical_pad, 0, quarters, &candidate.pad_text))
		return false;
	*metrics = candidate;
	return true;
}

bool form_scale_cell(unsigned int quarters, int logical_font, int actual_font,
	int units, bool grid, const form_scale_metrics *metrics,
	int *cell, int *row_height)
{
	int width, height;
	int64_t logical_width, actual_width, logical_height, actual_height;

	*cell = *row_height = 0;
	if (metrics == NULL || logical_font < 1 || actual_font < 1 ||
	    units < 1 || units > 4096 || metrics->text < 1)
		return false;
	logical_width = (int64_t)logical_font * units / 2;
	actual_width = (int64_t)actual_font * units / 2;
	logical_height = grid ? 3LL * logical_font + 12 : logical_font + 6LL;
	actual_height = grid ? 3LL * actual_font + 4LL * metrics->text :
		actual_font + 2LL * metrics->text;
	if (logical_width > INT_MAX || actual_width > INT_MAX ||
	    logical_height > INT_MAX || actual_height > INT_MAX ||
	    !form_scale_extent(logical_width, actual_width, quarters, &width) ||
	    !form_scale_extent(logical_height, actual_height,
		quarters, &height) ||
	    width < 1 || height < 1)
		return false;
	*cell = width;
	*row_height = height;
	return true;
}
