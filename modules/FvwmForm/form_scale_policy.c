/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "form_scale.h"
#include "form_workarea.h"
#include "libs/monitor_scale.h"
#include "libs/Parse.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

void form_workarea_sync(form_workarea_state *state,
	const module_ui_scale_snapshot *snapshot)
{
	if (state->epoch != snapshot->epoch)
	{
		*state = (form_workarea_state){0};
		state->epoch = snapshot->epoch;
	}
}

static bool form_workarea_contained(const module_monitor_area *physical,
	const module_monitor_area *area)
{
	return physical->width > 0 && physical->height > 0 &&
		(int64_t)physical->x + physical->width <= INT_MAX &&
		(int64_t)physical->y + physical->height <= INT_MAX &&
		area->width > 0 && area->height > 0 &&
		area->x >= physical->x && area->y >= physical->y &&
		(int64_t)area->x + area->width <=
			(int64_t)physical->x + physical->width &&
		(int64_t)area->y + area->height <=
			(int64_t)physical->y + physical->height;
}

form_workarea_status form_workarea_receive(form_workarea_state *state,
	const module_ui_scale_snapshot *snapshot, const char *text)
{
	char line[MODULE_UI_SCALE_LINE_MAX], name[MODULE_UI_SCALE_NAME_MAX + 1];
	char *token, *tail;
	int values[MODULE_MONITOR_VALUES];
	module_monitor_area physical, area;
	size_t length;
	unsigned int index;

	if (text == NULL || strncmp(text, "Monitor", 7) != 0 ||
	    !isspace((unsigned char)text[7]))
		return FORM_WORKAREA_IGNORED;
	length = strnlen(text, sizeof(line));
	if (length == sizeof(line) || snapshot->epoch == 0 ||
	    snapshot->count > MODULE_UI_SCALE_MAX)
		return FORM_WORKAREA_REFUSED;
	form_workarea_sync(state, snapshot);
	memcpy(line, text, length + 1);
	token = PeekToken(line + 7, &tail);
	if (token == NULL || strlen(token) > MODULE_UI_SCALE_NAME_MAX)
		return FORM_WORKAREA_REFUSED;
	strcpy(name, token);
	for (index = 0; index < snapshot->count; index++)
		if (strcmp(snapshot->rows[index].name, name) == 0)
			break;
	if (index == snapshot->count)
		return FORM_WORKAREA_REFUSED;
	for (unsigned int i = 0; i < MODULE_MONITOR_VALUES; i++)
	{
		const char *digit;

		token = PeekToken(tail, &tail);
		if (token == NULL)
			return FORM_WORKAREA_REFUSED;
		digit = token;
		if (*digit == '+' || *digit == '-')
			digit++;
		if (*digit == '\0')
			return FORM_WORKAREA_REFUSED;
		for (; *digit != '\0'; digit++)
			if (*digit < '0' || *digit > '9')
				return FORM_WORKAREA_REFUSED;
		if (!module_parse_decimal_integers(token, &values[i], 1))
			return FORM_WORKAREA_REFUSED;
	}
	if (PeekToken(tail, NULL) != NULL || values[10] < 0 || values[11] < 0 ||
	    values[12] < 0 || values[13] < 0)
		return FORM_WORKAREA_REFUSED;
	physical = (module_monitor_area){snapshot->rows[index].rect.x,
		snapshot->rows[index].rect.y, snapshot->rows[index].rect.width,
		snapshot->rows[index].rect.height};
	if (!module_monitor_prepare_area(values, &physical, false, &area) ||
	    !form_workarea_contained(&physical, &area))
		return FORM_WORKAREA_REFUSED;
	state->areas[index] = area;
	state->valid[index] = true;
	return FORM_WORKAREA_ADMITTED;
}

bool form_workarea_bounds(const form_workarea_state *state,
	const module_ui_scale_snapshot *snapshot, const module_ui_scale_row *row,
	module_monitor_area *area)
{
	module_monitor_area physical;

	*area = (module_monitor_area){0};
	if (row == NULL || snapshot->epoch == 0 ||
	    snapshot->count > MODULE_UI_SCALE_MAX)
		return false;
	physical = (module_monitor_area){row->rect.x, row->rect.y,
		row->rect.width, row->rect.height};
	if (!form_workarea_contained(&physical, &physical))
		return false;
	for (unsigned int i = 0; i < snapshot->count; i++)
		if (row == &snapshot->rows[i])
		{
			*area = physical;
			if (state->epoch == snapshot->epoch && state->valid[i] &&
			    form_workarea_contained(&physical, &state->areas[i]))
				*area = state->areas[i];
			return true;
		}
	return false;
}

bool form_scale_initial_size(int logical_width, int rows,
	unsigned int quarters, int fixed, int row_height, int border,
	const form_scale_size *minimum, const form_scale_size *available,
	form_scale_size *size)
{
	form_scale_size candidate;
	int64_t height = (int64_t)fixed + (int64_t)rows * row_height + border;

	*size = (form_scale_size){0};
	if (minimum == NULL || minimum->width < 1 || minimum->width > 32767 ||
	    minimum->height < 1 || minimum->height > 32767 ||
	    logical_width < 1 || rows < 1 ||
	    rows > 32 || fixed < 0 || row_height < 1 || border < 0 ||
	    height < 1 || height > 32767 ||
	    !form_scale_extent(logical_width, minimum->width,
		quarters, &candidate.width))
		return false;
	candidate.height = height > minimum->height ? height : minimum->height;
	if (available != NULL)
	{
		if (available->width < 1 || available->height < 1)
			return false;
		if (candidate.width > available->width)
			candidate.width = available->width;
		if (candidate.height > available->height)
			candidate.height = available->height;
		if (candidate.width < minimum->width)
			candidate.width = minimum->width;
		if (candidate.height < minimum->height)
			candidate.height = minimum->height;
	}
	*size = candidate;
	return true;
}

bool form_scale_startup_axis(int origin, int extent, int bound_origin,
	int bound_extent, int *position)
{
	int64_t end = (int64_t)bound_origin + bound_extent - extent;
	int64_t candidate = origin;

	*position = 0;
	if (extent < 1 || extent > 32767 || bound_extent < 1)
		return false;
	if (extent >= bound_extent || candidate < bound_origin)
		candidate = bound_origin;
	else if (candidate > end)
		candidate = end;
	if (candidate < -32768 || candidate > 32767)
		return false;
	*position = candidate;
	return true;
}

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
