#ifndef FVWM_FORM_SCALE_H
#define FVWM_FORM_SCALE_H

#include <stdbool.h>

#define FORM_SCALE_ITEMS 256
#define FORM_SCALE_TABLES 32

typedef struct
{
	int text, box, horizontal, vertical, gutter;
	int thumb_width, thumb_inset, thumb_min, pad_text;
} form_scale_metrics;

/* Exact logical sources and actual returned font metrics are independent. */
bool form_scale_metrics_prepare(unsigned int quarters, int logical_pad,
	form_scale_metrics *metrics);
bool form_scale_extent(int logical, int required, unsigned int quarters,
	int *extent);
bool form_scale_cell(unsigned int quarters, int logical_font, int actual_font,
	int units, bool grid, const form_scale_metrics *metrics,
	int *cell, int *row_height);

void form_scale_configure(char *text);
bool form_scale_message(const char *text);
void form_scale_end_config(void);
void form_scale_capture(const char *module_name);
void form_scale_initialize(const char *module_name);
void form_scale_retry(void);
bool form_scale_holds_view(void);
void form_scale_cleanup(void);
void form_scale_status(char *text);
bool form_scale_list_geometry(int units, int columns, int *cell, int *height);

#endif
