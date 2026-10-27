/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "form_scale.h"
#include "libs/monitor_scale.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

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
	    !form_scale_extent(1, 1, quarters, &candidate.stroke) ||
	    !form_scale_extent(logical_pad, 0, quarters, &candidate.pad_text))
		return false;
	*metrics = candidate;
	return true;
}

bool form_label_viewport_width(int minimum, int parent, int margin, int *width)
{
	*width = 0;
	if (minimum < 1 || parent < 1 || parent > 32767 || margin < 0 ||
	    2LL * margin + minimum > parent)
		return false;
	*width = parent - 2 * margin;
	return true;
}

bool form_label_fit(const char *source, int length, int available,
	void *context, int (*next_character)(void *, const char *),
	int (*measure)(void *, const char *, int),
	char display[FORM_LABEL_BYTES + 4], int *display_length)
{
	int offsets[FORM_LABEL_BYTES + 1];
	int full, count = 0, best = 0, low, high, measured;

	*display_length = 0;
	display[0] = '\0';
	if (source == NULL || length < 0 || length > FORM_LABEL_BYTES ||
	    available < 0 || next_character == NULL || measure == NULL)
		return false;
	full = measure(context, source, length);
	if (full < 0)
		return false;
	if (full <= available)
	{
		memcpy(display, source, length);
		display[length] = '\0';
		*display_length = length;
		return true;
	}
	offsets[0] = 0;
	for (int prefix = 0; prefix < length; )
	{
		int next = next_character(context, source + prefix);

		if (next < 1 || next > length - prefix)
			goto invalid;
		prefix += next;
		offsets[++count] = prefix;
	}
	memcpy(display, "...", 4);
	measured = measure(context, display, 3);
	if (measured < 0)
		goto invalid;
	if (measured > available)
	{
		display[0] = '\0';
		return true;
	}
	/* At most 12 font measurements for 1024 bytes. Shaping/kerning may be
	 * nonmonotonic, so this search can choose a shorter prefix than optimal.
	 * Every admitted candidate, including the suffix, is actually measured. */
	low = 1;
	high = count - 1;
	while (low <= high)
	{
		int middle = low + (high - low) / 2;
		int prefix = offsets[middle];

		memcpy(display, source, prefix);
		memcpy(display + prefix, "...", 4);
		measured = measure(context, display, prefix + 3);
		if (measured < 0)
			goto invalid;
		if (measured <= available)
		{
			best = prefix;
			low = middle + 1;
		}
		else
			high = middle - 1;
	}
	memcpy(display, source, best);
	memcpy(display + best, "...", 4);
	*display_length = best + 3;
	return true;
invalid:
	display[0] = '\0';
	return false;
}

int form_focus_stroke(int requested, int width, int height)
{
	int edge = width < height ? width : height;
	int limit;

	if (edge < 1)
		return 0;
	limit = edge / 2 + edge % 2;
	if (limit > 4)
		limit = 4;
	return requested < 1 ? 1 : requested > limit ? limit : requested;
}

bool form_caret_geometry(int x, int width, int height, int inset, int stroke,
	form_caret_rectangle *rectangle)
{
	*rectangle = (form_caret_rectangle){0};
	if (inset < 0 || 2LL * inset >= width || 2LL * inset >= height)
		return false;
	if (stroke < 1)
		stroke = 1;
	if (stroke > 4)
		stroke = 4;
	if (stroke > width - 2 * inset)
		stroke = width - 2 * inset;
	if (x < inset)
		x = inset;
	if (x > width - inset - stroke)
		x = width - inset - stroke;
	*rectangle = (form_caret_rectangle){
		x, inset, stroke, height - 2 * inset};
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
