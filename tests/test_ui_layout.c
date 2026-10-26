/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "libs/ui_layout.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

static void row_contract(void)
{
	int minimum[] = {100, 40, 80};
	unsigned weight[] = {1, 2, 1};
	int widths[] = {-1, -1, -1};
	bool clipped = true;

	assert(ui_layout_row(300, minimum, weight, 3, widths, &clipped));
	assert(!clipped && widths[0] == 120 && widths[1] == 80 &&
		widths[2] == 100);
	assert(ui_layout_row(220, minimum, weight, 3, widths, &clipped));
	assert(!clipped && !memcmp(widths, minimum, sizeof(widths)));
	assert(ui_layout_row(3, minimum, weight, 3, widths, &clipped));
	assert(clipped && widths[0] == 1 && widths[1] == 1 && widths[2] == 1);
	assert(ui_layout_row(100, minimum, weight, 3, widths, &clipped));
	assert(clipped && widths[0] > widths[1] && widths[2] > widths[1]);
	assert(widths[0] + widths[1] + widths[2] == 100);
	minimum[0] = 0;
	int old[3];
	memcpy(old, widths, sizeof(old));
	clipped = false;
	assert(!ui_layout_row(100, minimum, weight, 3, widths, &clipped));
	assert(!memcmp(old, widths, sizeof(old)) && !clipped);
	minimum[0] = 100;
	weight[0] = UINT_MAX;
	assert(!ui_layout_row(100, minimum, weight, 3, widths, &clipped));
	assert(!ui_layout_row(2, minimum, weight, 3, widths, &clipped));
	assert(!ui_layout_row(INT_MAX, minimum, weight, 3, widths, &clipped));
}

static void exhaustive_rows(void)
{
	int minimum[UI_LAYOUT_MAX_ITEMS];
	unsigned weight[UI_LAYOUT_MAX_ITEMS];
	int widths[UI_LAYOUT_MAX_ITEMS];
	bool clipped;

	for (size_t count = 1; count <= UI_LAYOUT_MAX_ITEMS; count++)
	{
		for (size_t i = 0; i < count; i++)
		{
			minimum[i] = (i * 131 + 7) % 32767 + 1;
			weight[i] = (i * 97) % 32768;
		}
		for (int pass = 0; pass < 4; pass++)
		{
			int available = pass == 0 ? (int)count :
				pass == 1 ? 4096 : pass == 2 ? 16384 : 32767;
			int sum = 0;
			assert(ui_layout_row(available, minimum, weight, count,
				widths, &clipped));
			for (size_t i = 0; i < count; i++)
			{
				assert(widths[i] > 0);
				sum += widths[i];
			}
			assert(sum == available);
		}
	}
	minimum[0] = minimum[1] = minimum[2] = 1;
	weight[0] = weight[1] = weight[2] = 0;
	assert(ui_layout_row(11, minimum, weight, 3, widths, &clipped));
	assert(!clipped && widths[0] == 3 && widths[1] == 4 && widths[2] == 4);
}

static void grid_contract(void)
{
	ui_layout_grid_result result = {9, 9};

	assert(ui_layout_grid(800, 320, 200, 40, 1, 8, 32, &result));
	assert(result.columns == 4 && result.rows == 8);
	assert(ui_layout_grid(1, 1, 200, 40, 1, 8, 32, &result));
	assert(result.columns == 1 && result.rows == 1);
	assert(ui_layout_grid(32767, 32767, 1, 1, 1, 8, 32, &result));
	assert(result.columns == 8 && result.rows == 32);
	result.columns = result.rows = 9;
	assert(!ui_layout_grid(0, 300, 200, 40, 1, 8, 32, &result));
	assert(!ui_layout_grid(800, 300, 0, 40, 1, 8, 32, &result));
	assert(!ui_layout_grid(800, 300, 200, 40, 8, 1, 32, &result));
	assert(!ui_layout_grid(800, 300, 200, 40, 1, 9, 32, &result));
	assert(!ui_layout_grid(800, 300, 200, 40, 1, 8, 33, &result));
	assert(!ui_layout_grid(1, 300, 200, 40, 2, 8, 32, &result));
	assert(result.columns == 9 && result.rows == 9);
}

int main(void)
{
	row_contract();
	exhaustive_rows();
	grid_contract();
	return 0;
}
