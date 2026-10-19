#ifndef FVWM_STYLE_TYPES_H
#define FVWM_STYLE_TYPES_H

#include "fvwm.h"
#include "style_id.h"
#include "style_match_types.h"

/* Special flags used by style evaluation and window setup. */
typedef struct style_flags
{
	common_flags_t common;
	unsigned do_decorate_transient : 1;
	/* old placement flags */
#define PLACE_DUMB            0x0
#define PLACE_SMART           0x1
#define PLACE_CLEVER          0x2
#define PLACE_CLEVERNESS_MASK 0x3
#define PLACE_RANDOM          0x4
	/* new placements value, try to get a minimal backward compatibility
	 * with the old flags:
	 * Dumb+Active = Manual,
	 * Dumb+Random = Cascade,
	 * Smart+Random = TileCascade,
	 * Smart+Active = TileManual,
	 * Random+Smart+Clever = MINOVERLAP which is the original Clever
	 * placement code,
	 * Active+Smart+Clever = MINOVERLAPPERCENT which is the "new" Clever
	 * placement code and was the original Clever placement code. Now the
	 * original placement code said:
	 * Active/Random+Dumb+Clever = Active/Random+Dumb (with Dumb Clever is
	 * ignored); These represent the not use value: 0x2=Active+Dumb+Clever,
	 * 0x6=Random+Dumb+Clever */
#define PLACE_MANUAL            0x0
#define PLACE_TILEMANUAL        0x1
#define PLACE_MANUAL_B          0x2
#define PLACE_MINOVERLAPPERCENT 0x3
#define PLACE_CASCADE           0x4
#define PLACE_TILECASCADE       0x5
#define PLACE_CASCADE_B         0x6
#define PLACE_MINOVERLAP        0x7
#define PLACE_POSITION          0x8
#define PLACE_MASK              0xF
	unsigned placement_mode : 4;
	unsigned ewmh_placement_mode : 2; /* see ewmh.h */
#define WS_CR_MOTION_METHOD_AUTO CR_MOTION_METHOD_AUTO
#define WS_CR_MOTION_METHOD_USE_GRAV CR_MOTION_METHOD_USE_GRAV
#define WS_CR_MOTION_METHOD_STATIC_GRAV CR_MOTION_METHOD_STATIC_GRAV
#define WS_CR_MOTION_METHOD_MASK CR_MOTION_METHOD_MASK
	unsigned ws_cr_motion_method : 2;
	unsigned do_save_under : 1;
	unsigned do_start_iconic : 1;
	unsigned do_start_lowered : 1;
	unsigned do_start_shaded : 1;
	unsigned start_shaded_dir : 3;
	unsigned has_border_width : 1;
	unsigned has_color_back : 1;
	unsigned has_color_fore : 1;
	unsigned has_color_back_hi : 1;
	unsigned has_color_fore_hi : 1;
	unsigned has_decor : 1;
	unsigned has_edge_delay_ms_move : 1;
	unsigned has_edge_delay_ms_resize : 1;
	unsigned has_edge_resistance_move : 1;
	unsigned has_edge_resistance_screen_move : 1;
	unsigned has_handle_width : 1;
	unsigned has_corner_length : 1;
	unsigned has_icon : 1;
	unsigned has_icon_boxes : 1;
	unsigned has_icon_size_limits : 1;
	unsigned has_min_window_size : 1;
	unsigned has_max_window_size : 1;
	unsigned has_icon_background_padding : 1;
	unsigned has_icon_background_relief : 1;
	unsigned has_icon_title_relief : 1;
	unsigned has_window_shade_steps : 1;
	unsigned has_mini_icon : 1;
	unsigned has_mwm_decor : 1;
	unsigned has_mwm_functions : 1;
	unsigned has_no_handles : 1;
	unsigned has_no_title : 1;
	unsigned has_ol_decor : 1;
	unsigned has_snap_grid : 1;
	unsigned has_snap_attraction : 1;
#if 0
	unsigned has_condition_mask : 1;
#endif
	unsigned is_button_disabled : NUMBER_OF_TITLE_BUTTONS;
	unsigned is_unmanaged : 1;
#define BACKINGSTORE_DEFAULT 0
#define BACKINGSTORE_ON      1
#define BACKINGSTORE_OFF     2
#define BACKINGSTORE_MASK  0x3
	unsigned use_backing_store : 2;
	unsigned use_parent_relative : 1;
	unsigned use_colorset : 1;
	unsigned use_colorset_hi : 1;
	unsigned use_border_colorset : 1;
	unsigned use_border_colorset_hi : 1;
	unsigned use_icon_title_colorset : 1;
	unsigned use_icon_title_colorset_hi : 1;
	unsigned use_icon_background_colorset : 1;
	unsigned use_layer : 1;
	unsigned use_no_pposition : 1;
	unsigned use_no_usposition : 1;
	unsigned use_no_transient_pposition : 1;
	unsigned use_no_transient_usposition : 1;
	unsigned use_start_on_desk : 1;
	unsigned use_start_on_page_for_transient : 1;
	unsigned use_start_on_screen : 1;
	unsigned manual_placement_honors_starts_on_page : 1;
	unsigned um_placement_honors_starts_on_page : 1;
	unsigned capture_honors_starts_on_page : 1;
	unsigned recapture_honors_starts_on_page : 1;
	unsigned has_placement_penalty : 1;
	unsigned has_placement_percentage_penalty : 1;
	unsigned has_placement_position_string : 1;
	unsigned has_initial_map_command_string : 1;
	unsigned has_title_format_string : 1;
	unsigned has_icon_title_format_string : 1;
	unsigned initial_placement_done : 1;
} style_flags;

/* Mutable rule/result layout; include only when accessing its fields. */
struct window_style
{
	struct window_style *next;
	struct window_style *prev;
	style_id_t id;
	style_match_patterns id_patterns;
	/* Reassigned before each simplification; never a retained lookup key. */
	size_t simplify_ordinal;
	/* Derived chains are valid only while the owning list has an index. */
	struct window_style *lookup_next;
	struct window_style *lookup_prev;
	size_t lookup_ordinal;
#if 0
	WindowConditionMask *condition_mask;
#endif
	char *icon_name;
	char *mini_icon_name;
	char *decor_name;
	unsigned char min_icon_width;
	unsigned char max_icon_width;
	unsigned char min_icon_height;
	unsigned char max_icon_height;
#define ICON_RESIZE_TYPE_NONE      0x0
#define ICON_RESIZE_TYPE_STRETCHED 0x1
#define ICON_RESIZE_TYPE_ADJUSTED  0x2
#define ICON_RESIZE_TYPE_SHRUNK    0x3
#define ICON_RESIZE_TYPE_MASK      0x3
	unsigned icon_resize_type : 2;
	unsigned char icon_background_padding;
	signed char icon_background_relief;
	signed char icon_title_relief;
	char *icon_font;
	char *window_font;
	int colorset;
	int colorset_hi;
	int border_colorset[BP_SIZE];
	int border_colorset_hi[BP_SIZE];
	int icon_title_colorset;
	int icon_title_colorset_hi;
	int icon_background_colorset;
	short border_width;
	/* resize handle width */
	short handle_width;
	short corner_length;
	int layer;
	int start_desk;
	int start_page_x;
	int start_page_y;
	char *start_screen;
	int min_window_width;
	int min_window_height;
	int max_window_width;
	int max_window_height;
	/* sizes are in terms of client window size hints */
	bool min_window_width_is_c;
	bool min_window_height_is_c;
	bool max_window_width_is_c;
	bool max_window_height_is_c;
	int shade_anim_steps;
	snap_attraction_t snap_attraction;
	/* snap grid size */
	int snap_grid_x;
	int snap_grid_y;
	int edge_delay_ms_move;
	int edge_delay_ms_resize;
	int edge_resistance_move;
	int edge_resistance_screen_move;
	icon_boxes *icon_boxes;
	float norm_placement_penalty;
	pl_penalty_struct pl_penalty;
	pl_percent_penalty_struct pl_percent_penalty;
	char *pl_position_string;
	char *initial_map_command_string;
	char *title_format_string;
	char *icon_title_format_string;
	style_flags flags;
	style_flags flag_default;
	style_flags flag_mask;
	style_flags change_mask;
	unsigned has_style_changed : 1;
	unsigned has_title_format_string : 1;
	unsigned has_icon_title_format_string : 1;
};

#endif
