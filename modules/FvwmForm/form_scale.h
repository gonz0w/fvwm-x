#ifndef FVWM_FORM_SCALE_H
#define FVWM_FORM_SCALE_H

#include <stdbool.h>

#define FORM_SCALE_ITEMS 256
#define FORM_SCALE_TABLES 32

typedef struct
{
	int text, box, horizontal, vertical, gutter;
	int thumb_width, thumb_inset, thumb_min, pad_text;
	int stroke;
} form_scale_metrics;

#define FORM_LABEL_BYTES 1024

/* Measurement includes the suffix; locale character boundaries come from the
 * same actual font as measurement. The original value is never modified. */
bool form_label_fit(const char *source, int length, int available,
	void *context, int (*next_character)(void *, const char *),
	int (*measure)(void *, const char *, int),
	char display[FORM_LABEL_BYTES + 4], int *display_length);
bool form_label_viewport_width(int minimum, int parent, int margin, int *width);
int form_focus_stroke(int requested, int width, int height);
typedef struct
{
	int x, y, width, height;
} form_caret_rectangle;
bool form_caret_geometry(int x, int width, int height, int inset, int stroke,
	form_caret_rectangle *rectangle);

/* Exact logical sources and actual returned font metrics are independent. */
bool form_scale_metrics_prepare(unsigned int quarters, int logical_pad,
	form_scale_metrics *metrics);
bool form_scale_extent(int logical, int required, unsigned int quarters,
	int *extent);
bool form_scale_cell(unsigned int quarters, int logical_font, int actual_font,
	int units, bool grid, const form_scale_metrics *metrics,
	int *cell, int *row_height);

typedef struct
{
	int width, height;
} form_scale_size;

/* Preferred startup geometry is separate from the one-row legal minimum.
 * An available physical span may reduce preference, never readable minima. */
bool form_scale_initial_size(int logical_width, int rows,
	unsigned int quarters, int fixed, int row_height, int border,
	const form_scale_size *minimum, const form_scale_size *available,
	form_scale_size *size);
bool form_scale_startup_axis(int origin, int extent, int bound_origin,
	int bound_extent, int *position);

void form_scale_configure(char *text);
bool form_scale_message(const char *text);
void form_scale_end_config(void);
void form_scale_capture(const char *module_name);
void form_scale_initialize(const char *module_name);
void form_scale_startup_position(void);
void form_scale_retry(void);
bool form_scale_holds_view(void);
void form_scale_cleanup(void);
void form_scale_status(char *text);
bool form_scale_list_geometry(int units, int columns, int *cell, int *height);

#endif
