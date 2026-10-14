/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "pager_geometry.h"
#include "libs/module_monitor.h"
#include <limits.h>



static bool pager_prepare_pages(int x_pages, int y_pages,
	int screen_width, int screen_height,
	pager_desktop_geometry *result)
{
	int pages[2] = {x_pages, y_pages};

	if (result == NULL)
	{
		return false;
	}
	*result = (pager_desktop_geometry){0};
	if (screen_width <= 0 || screen_height <= 0)
	{
		return false;
	}
	for (int i = 0; i < 2; i++)
	{
		if (pages[i] <= 0)
		{
			pages[i] = 1;
		}
	}
	if (pages[0] > INT_MAX / screen_width ||
		pages[1] > INT_MAX / screen_height)
	{
		return false;
	}
	result->width = pages[0] * screen_width;
	result->height = pages[1] * screen_height;
	result->max_x = result->width - screen_width;
	result->max_y = result->height - screen_height;
	result->x_pages = pages[0];
	result->y_pages = pages[1];
	return true;
}

bool pager_prepare_desktop(const char *text, int screen_width, int screen_height,
	pager_desktop_geometry *result)
{
	int pages[2];

	if (result == NULL)
	{
		return false;
	}
	*result = (pager_desktop_geometry){0};
	return module_parse_decimal_integers(text, pages, 2) &&
		pager_prepare_pages(pages[0], pages[1], screen_width,
			screen_height, result);
}

bool pager_prepare_monitor(const char *text, pager_monitor_geometry *result)
{
	int values[14];
	pager_desktop_geometry desktop;

	if (result == NULL)
	{
		return false;
	}
	*result = (pager_monitor_geometry){0};
	if (!module_parse_decimal_integers(text, values, 14) ||
		!pager_prepare_pages(values[1], values[2], values[8],
			values[9], &desktop))
	{
		return false;
	}
	*result = (pager_monitor_geometry){values[0], values[1], values[2],
		values[3], values[4], values[5], values[6], values[7],
		values[8], values[9], desktop};
	return true;
}

bool pager_prepare_grid(int desks, int rows, int columns,
	int *prepared_rows, int *prepared_columns)
{
	if (desks <= 0 || prepared_rows == NULL || prepared_columns == NULL)
	{
		return false;
	}
	if (rows < 0)
	{
		if (columns <= 0)
		{
			columns = desks;
			rows = 1;
		}
		else
		{
			rows = desks / columns + (desks % columns != 0);
		}
	}
	if (columns < 0)
	{
		if (rows == 0)
			rows = 1;
		columns = desks / rows + (desks % rows != 0);
	}
	/* Division avoids both a grid product and overflowing rounded addition. */
	if (rows == 0 || columns == 0 ||
		rows < desks / columns + (desks % columns != 0))
	{
		if (columns == 0)
			columns = 1;
		rows = desks / columns + (desks % columns != 0);
	}
	*prepared_rows = rows;
	*prepared_columns = columns;
	return true;
}

bool pager_prepare_scale(const char *text, unsigned int *scale)
{
	int parsed;

	if (scale == NULL || !module_parse_decimal_integers(text, &parsed, 1) ||
		parsed <= 0)
	{
		return false;
	}
	*scale = (unsigned int)parsed;
	return true;
}

int pager_pixel_size(int64_t value)
{
	return value < 1 ? 1 : value > 65535 ? 65535 : (int)value;
}

int pager_pixel_position(int64_t value)
{
	return value < -32768 ? -32768 : value > 32767 ? 32767 : (int)value;
}

static int pager_scaled_extent(int pages, int pixels, int cells,
	unsigned int scale)
{
	/* Compare before the third multiply. Its divided result can only be
	 * representable when the product fits this bounded numerator. */
	uint64_t product = (uint64_t)pages * (unsigned int)pixels;
	uint64_t ceiling = (uint64_t)65535 * scale;

	if (product > ceiling / (unsigned int)cells)
	{
		return 65535;
	}
	return pager_pixel_size((int64_t)(product * (unsigned int)cells / scale) +
		cells);
}

bool pager_prepare_window_size(int width, int height, int rows, int columns,
	int label_height, int x_pages, int y_pages, int virtual_width,
	int virtual_height, unsigned int scale, int *prepared_width,
	int *prepared_height)
{
	if (rows <= 0 || columns <= 0 || label_height < 0 || x_pages <= 0 ||
		y_pages <= 0 || virtual_width <= 0 || virtual_height <= 0 ||
		scale == 0 || prepared_width == NULL || prepared_height == NULL)
	{
		return false;
	}
	int64_t label_space = (int64_t)label_height * rows;
	int next_width = width;
	int next_height = height;
	if (width <= 0 || height <= 0)
	{
		if (width > 0)
		{
			next_height = pager_pixel_size((int64_t)width *
				virtual_height / virtual_width + label_space + rows);
		}
		else if (height > label_space)
		{
			next_width = pager_pixel_size(((int64_t)height - label_space +
				rows) * virtual_width / virtual_height + columns);
		}
		else
		{
			next_width = pager_scaled_extent(x_pages, virtual_width,
				columns, scale);
			next_height = pager_pixel_size((int64_t)pager_scaled_extent(
				y_pages, virtual_height, rows, scale) + label_space);
		}
	}
	*prepared_width = pager_pixel_size(next_width);
	*prepared_height = pager_pixel_size(next_height);
	return true;
}

bool pager_prepare_cell_size(int width, int height, int rows, int columns,
	int label_height, int *cell_width, int *cell_height)
{
	if (rows <= 0 || columns <= 0 || label_height < 0 ||
		cell_width == NULL || cell_height == NULL)
	{
		return false;
	}
	*cell_width = pager_pixel_size(((int64_t)width - columns + 1) / columns);
	*cell_height = pager_pixel_size(((int64_t)height -
		(int64_t)rows * label_height - rows + 1) / rows);
	return true;
}

int pager_round_icon_size(int size, int pages)
{
	if (pages < 1)
	{
		pages = 1;
	}
	int64_t step = (int64_t)pages + 1;
	return pager_pixel_size((size / step) * step + pages);
}

bool pager_prepare_viewport_axis(int64_t position, int span, int scale, int full,
	int *origin, int *extent)
{
	if (origin == NULL || extent == NULL)
	{
		return false;
	}
	*origin = 0;
	*extent = 1;
	if (full <= 0 || scale <= 0 || scale > 65535 || span <= 0 ||
		position < (int64_t)INT_MIN * 2 ||
		position > (int64_t)INT_MAX * 2)
	{
		return false;
	}
	int64_t start = (int64_t)position * scale / full;
	int64_t end = ((int64_t)position + span) * scale / full;
	*origin = pager_pixel_position(start);
	*extent = pager_pixel_size(end - start);
	return true;
}

int pager_integer_value(int64_t value)
{
	return value < INT_MIN ? INT_MIN : value > INT_MAX ? INT_MAX : (int)value;
}

static bool pager_projection_is_valid(const pager_projection_axis *axis)
{
	return axis != NULL && axis->all_pixels > 0 && axis->pages > 0 &&
		axis->scale > 0 && axis->scale <= 65535 &&
		(!axis->tracked || (axis->monitor_pixels > 0 &&
		axis->monitor_pixels <= axis->all_pixels));
}

bool pager_project_axis(int64_t position, int span,
	const pager_projection_axis *axis, int *origin, int *extent)
{
	if (origin == NULL || extent == NULL)
	{
		return false;
	}
	*origin = -32768;
	*extent = 1;
	if (!pager_projection_is_valid(axis) || span <= 0 ||
		position < (int64_t)INT_MIN * 2 ||
		position > (int64_t)INT_MAX * 2)
	{
		return false;
	}
	int pixels = axis->all_pixels;
	if (axis->tracked)
	{
		/* q * monitor_pixels cannot exceed the original position's
		 * magnitude, because a monitor fits within the full layout. */
		position = (position / pixels) * axis->monitor_pixels +
			position % pixels - axis->monitor_origin;
		pixels = axis->monitor_pixels;
	}
	int64_t full = (int64_t)pixels * axis->pages;
	int64_t start = position * axis->scale / full;
	int64_t end = (position + span) * axis->scale / full;
	*origin = pager_pixel_position(start);
	*extent = pager_pixel_size(end - start);
	return true;
}

/* Saturate only after evaluating the signed result; no overflowing product
 * or premature clamp can change an ordinary representable coordinate. */
static int pager_product_sum(int64_t value, int factor, int64_t correction)
{
	if (value > INT64_MAX / factor)
	{
		return INT_MAX;
	}
	if (value < INT64_MIN / factor)
	{
		return INT_MIN;
	}
	int64_t product = value * factor;
	if (product > (int64_t)INT_MAX - correction)
	{
		return INT_MAX;
	}
	if (product < (int64_t)INT_MIN - correction)
	{
		return INT_MIN;
	}
	return (int)(product + correction);
}

bool pager_restore_axis(int position, int span,
	const pager_projection_axis *axis, int *origin, int *extent)
{
	if (origin == NULL || extent == NULL)
	{
		return false;
	}
	*origin = 0;
	*extent = 1;
	if (!pager_projection_is_valid(axis) || span <= 0)
	{
		return false;
	}
	int pixels = axis->tracked ? axis->monitor_pixels : axis->all_pixels;
	int64_t numerator = (int64_t)position * axis->pages;
	int64_t correction = (numerator % axis->scale) * pixels / axis->scale;
	if (axis->tracked)
	{
		correction += axis->monitor_origin;
	}
	*origin = pager_product_sum(numerator / axis->scale,
		axis->all_pixels, correction);
	int64_t full = (int64_t)pixels * axis->pages;
	correction = (full % axis->scale) * span / axis->scale;
	*extent = pager_product_sum(full / axis->scale, span, correction);
	if (*extent < 1)
	{
		*extent = 1;
	}
	return true;
}

bool pager_prepare_window_metric(const char *text, unsigned int *metric)
{
	int parsed;

	if (metric == NULL || !module_parse_decimal_integers(text, &parsed, 1) ||
		parsed < 0)
	{
		return false;
	}
	*metric = (unsigned int)parsed;
	return true;
}

unsigned int pager_minimum_size(unsigned int border, unsigned int content)
{
	return (unsigned int)pager_pixel_size((int64_t)border * 2 + content);
}
