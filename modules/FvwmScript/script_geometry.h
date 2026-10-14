#ifndef FVWMSCRIPT_SCRIPT_GEOMETRY_H
#define FVWMSCRIPT_SCRIPT_GEOMETRY_H

#include <stdbool.h>
#include <stdint.h>

/* X11 window requests use unsigned 16-bit extents and signed coordinates. */
bool script_geometry_size(int64_t width, int64_t height);
bool script_geometry_position(int64_t x, int64_t y);
/* Legacy zero startup widget extents become one before minimum sizing. */
bool script_geometry_startup_size(int width, int height,
	int *normalized_width, int *normalized_height);

typedef enum
{
	SCRIPT_LAYOUT_PUSH,
	SCRIPT_LAYOUT_RADIO,
	SCRIPT_LAYOUT_ITEM,
	SCRIPT_LAYOUT_CHECK,
	SCRIPT_LAYOUT_TEXT,
	SCRIPT_LAYOUT_HSCROLL,
	SCRIPT_LAYOUT_VSCROLL,
	SCRIPT_LAYOUT_POPUP,
	SCRIPT_LAYOUT_RECTANGLE,
	SCRIPT_LAYOUT_MINISCROLL,
	SCRIPT_LAYOUT_SWALLOW,
	SCRIPT_LAYOUT_HDIP,
	SCRIPT_LAYOUT_VDIP,
	SCRIPT_LAYOUT_LIST,
	SCRIPT_LAYOUT_MENU
} script_layout_type;

typedef struct
{
	script_layout_type type;
	int width, height, x, y;
	int font_height, text_width, second_text_width;
	int icon_width, icon_height;
	bool has_icon, has_title;
	int64_t menu_x;
} script_layout_input;

typedef struct
{
	int width, height, x, y;
} script_layout_geometry;

/* Preserve legacy minimum/fixed sizing; validate before narrowing or commit. */
bool script_geometry_layout(const script_layout_input *input,
	script_layout_geometry *geometry);

#endif
