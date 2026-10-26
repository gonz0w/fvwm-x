/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "ui_layout.h"

#include <stdint.h>

bool ui_layout_row(int available, const int *minimum,
	const unsigned *weights, size_t count, int *widths, bool *clipped)
{
	int prepared[UI_LAYOUT_MAX_ITEMS];
	uint64_t total = 0;
	uint64_t weight = 0;
	uint64_t consumed = 0;
	uint64_t previous = 0;
	int remaining;
	bool compress;

	if (!minimum || !weights || !widths || !clipped || count == 0 ||
	    count > UI_LAYOUT_MAX_ITEMS || available < (int)count ||
	    available > UI_LAYOUT_MAX_EXTENT)
		return false;
	for (size_t i = 0; i < count; i++)
	{
		if (minimum[i] < 1 || minimum[i] > UI_LAYOUT_MAX_EXTENT ||
		    weights[i] > UI_LAYOUT_MAX_EXTENT)
			return false;
		total += minimum[i];
		weight += weights[i];
	}
	compress = total > (uint64_t)available;
	remaining = available - (compress ? (int)count : (int)total);
	if (compress)
		weight = total - count;
	for (size_t i = 0; i < count; i++)
	{
		uint64_t share = compress ? (unsigned)(minimum[i] - 1) :
			weights[i];
		uint64_t current;

		/* Zero weights mean equal surplus, without inventing priority. */
		consumed += weight == 0 ? 1 : share;
		current = (uint64_t)remaining * consumed /
			(weight == 0 ? count : weight);
		prepared[i] = (compress ? 1 : minimum[i]) +
			(int)(current - previous);
		previous = current;
	}
	for (size_t i = 0; i < count; i++)
		widths[i] = prepared[i];
	*clipped = compress;
	return true;
}

bool ui_layout_grid(int width, int height, int minimum_cell, int row_height,
	int min_columns, int max_columns, int max_rows,
	ui_layout_grid_result *result)
{
	ui_layout_grid_result prepared;

	if (!result || width < 1 || width > UI_LAYOUT_MAX_EXTENT ||
	    height < 1 || height > UI_LAYOUT_MAX_EXTENT || minimum_cell < 1 ||
	    minimum_cell > UI_LAYOUT_MAX_EXTENT || row_height < 1 ||
	    row_height > UI_LAYOUT_MAX_EXTENT || min_columns < 1 ||
	    max_columns < min_columns || max_columns > 8 ||
	    width < min_columns ||
	    max_rows < 1 || max_rows > 32)
		return false;
	prepared.columns = width / minimum_cell;
	if (prepared.columns < min_columns)
		prepared.columns = min_columns;
	if (prepared.columns > max_columns)
		prepared.columns = max_columns;
	prepared.rows = height / row_height;
	if (prepared.rows < 1)
		prepared.rows = 1;
	if (prepared.rows > max_rows)
		prepared.rows = max_rows;
	*result = prepared;
	return true;
}
