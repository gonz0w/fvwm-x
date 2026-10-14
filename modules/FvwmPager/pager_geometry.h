#ifndef FVWM_PAGER_GEOMETRY_H
#define FVWM_PAGER_GEOMETRY_H

#include <stdbool.h>
#include <stdint.h>


typedef struct {
	int max_x;
	int max_y;
	int width;
	int height;
	int x_pages;
	int y_pages;
} pager_desktop_geometry;

typedef struct {
	int flags;
	int x_pages;
	int y_pages;
	int viewport_x;
	int viewport_y;
	int viewport_max_x;
	int viewport_max_y;
	int desk;
	int screen_width;
	int screen_height;
	pager_desktop_geometry desktop;
} pager_monitor_geometry;

/* Decimal-prefix parsing and the existing non-positive one-page fallback.
 * Prepare all derived values before publishing any monitor geometry. */
bool pager_prepare_desktop(const char *text, int screen_width, int screen_height,
	pager_desktop_geometry *result);

/* The core supplies fourteen integers, including four unused border values.
 * Prepare the complete message before refreshing or publishing monitor state. */
bool pager_prepare_monitor(const char *text, pager_monitor_geometry *result);

/* Resolve the existing row/column defaults without multiplying signed ints. */
bool pager_prepare_grid(int desks, int rows, int columns,
	int *prepared_rows, int *prepared_columns);

/* Scale is a positive divisor. Invalid input keeps the caller's prior scale. */
bool pager_prepare_scale(const char *text, unsigned int *scale);

/* Clamp derived rendering values at the X11 request boundary. */
int pager_pixel_size(int64_t value);
int pager_pixel_position(int64_t value);

/* Preserve ordinary aspect/scale rules with bounded derived X sizes. */
bool pager_prepare_window_size(int width, int height, int rows, int columns,
	int label_height, int x_pages, int y_pages, int virtual_width,
	int virtual_height, unsigned int scale, int *prepared_width,
	int *prepared_height);

bool pager_prepare_cell_size(int width, int height, int rows, int columns,
	int label_height, int *cell_width, int *cell_height);
int pager_round_icon_size(int size, int pages);
bool pager_prepare_viewport_axis(int64_t position, int span, int scale, int full,
	int *origin, int *extent);

typedef struct {
	int all_pixels;
	int monitor_pixels;
	int monitor_origin;
	int pages;
	int scale;
	bool tracked;
} pager_projection_axis;

/* Preserve endpoint rounding while preparing both directions of projection. */
bool pager_project_axis(int64_t position, int span,
	const pager_projection_axis *axis, int *origin, int *extent);
bool pager_restore_axis(int position, int span,
	const pager_projection_axis *axis, int *origin, int *extent);
int pager_integer_value(int64_t value);

/* Non-negative configuration values; zero keeps each directive's default rule. */
bool pager_prepare_window_metric(const char *text, unsigned int *metric);
unsigned int pager_minimum_size(unsigned int border, unsigned int content);

#endif
