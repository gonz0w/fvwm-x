#ifndef FVWM_WM_UI_METRICS_H
#define FVWM_WM_UI_METRICS_H

#include "libs/monitor_scale.h"

#include <stdbool.h>

#define WM_UI_RESOURCE_GENERATION_INITIAL 1U
#define WM_UI_XCURSOR_NOMINAL_SOURCE_PX 16

typedef struct
{
	unsigned int generation;
} wm_ui_resource_generation;

typedef struct
{
	unsigned int scale_quarters;
	int	     target_width;
	int	     target_height;
	unsigned int generation;
} wm_ui_cache_key;

/* Always convert from the retained exact source, never a rounded result. */
monitor_scale_conversion_status
wm_ui_scale_px(int source, monitor_scale_factor factor, int *px);

monitor_scale_conversion_status
wm_ui_compose_title_thickness(int scaled_font_height, int extra_source,
    int explicit_title_source, int min_title_source,
    monitor_scale_factor factor, int *thickness, int *offset);

monitor_scale_conversion_status
wm_ui_scale_hotspot(
    int source_hot, int extent, monitor_scale_factor factor, int *hot);

monitor_scale_conversion_status
wm_ui_scale_edge_thickness(
    int source, monitor_scale_factor factor, int *px);

bool
wm_ui_cache_key_equal(const wm_ui_cache_key *a, const wm_ui_cache_key *b);

wm_ui_resource_generation
wm_ui_resource_generation_current(void);

#endif
