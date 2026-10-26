#ifndef FVWM_UI_LAYOUT_H
#define FVWM_UI_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#define UI_LAYOUT_MAX_ITEMS 256
#define UI_LAYOUT_MAX_EXTENT 32767

typedef struct
{
	int columns;
	int rows;
} ui_layout_grid_result;

/* Positive adjacent widths cover available exactly. Insufficient space keeps
 * every action addressable and marks clipped; it never hides an action.
 * Invalid inputs leave all outputs untouched. */
bool ui_layout_row(int available, const int *minimum,
	const unsigned *weights, size_t count, int *widths, bool *clipped);

/* The caller supplies content pixels after padding/scrollbar subtraction.
 * Geometry is independent of collection identity and snapshot generation. */
bool ui_layout_grid(int width, int height, int minimum_cell, int row_height,
	int min_columns, int max_columns, int max_rows,
	ui_layout_grid_result *result);

#endif
