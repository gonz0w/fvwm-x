/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "script_geometry.h"

#include <stddef.h>

bool
script_geometry_size(int64_t width, int64_t height)
{
	return width >= 1 && width <= UINT16_MAX &&
	    height >= 1 && height <= UINT16_MAX;
}

bool
script_geometry_position(int64_t x, int64_t y)
{
	return x >= INT16_MIN && x <= INT16_MAX &&
	    y >= INT16_MIN && y <= INT16_MAX;
}

bool
script_geometry_startup_size(int width, int height,
	int *normalized_width, int *normalized_height)
{
	if (normalized_width != NULL)
	{
		*normalized_width = 0;
	}
	if (normalized_height != NULL)
	{
		*normalized_height = 0;
	}
	if (normalized_width == NULL || normalized_height == NULL)
	{
		return false;
	}
	width = width == 0 ? 1 : width;
	height = height == 0 ? 1 : height;
	if (!script_geometry_size(width, height))
	{
		return false;
	}
	*normalized_width = width;
	*normalized_height = height;
	return true;
}

static int64_t
larger(int64_t a, int64_t b)
{
	return a > b ? a : b;
}

bool
script_geometry_layout(const script_layout_input *input,
	script_layout_geometry *geometry)
{
	int64_t width, height, x, y, font, text, icon_width, icon_height;

	if (geometry == NULL)
	{
		return false;
	}
	*geometry = (script_layout_geometry){0};
	if (input == NULL)
	{
		return false;
	}
	width = input->width;
	height = input->height;
	x = input->x;
	y = input->y;
	font = input->font_height;
	text = input->text_width;
	icon_width = input->icon_width;
	icon_height = input->icon_height;
	/* All products below have int inputs and fit in int64_t. */
	switch (input->type)
	{
	case SCRIPT_LAYOUT_PUSH:
		if (!input->has_icon)
		{
			width = larger(width, text + 16);
			height = larger(height, font + 12);
		}
		else if (!input->has_title)
		{
			width = larger(width, icon_width + 10);
			height = larger(height, icon_height + 10);
		}
		else
		{
			width = larger(width, larger(icon_width + 10, text + 16));
			height = larger(height, icon_height + 2 * (font + 10));
		}
		break;
	case SCRIPT_LAYOUT_RADIO:
		width = text + 20;
		height = font + 5;
		break;
	case SCRIPT_LAYOUT_ITEM:
		if (!input->has_icon && input->has_title)
		{
			width = larger(width, text + 2);
			height = larger(height, font + 2);
		}
		else if (input->has_icon && !input->has_title)
		{
			width = larger(width, icon_width);
			height = larger(height, icon_height);
		}
		else if (input->has_icon)
		{
			width = larger(width, larger(icon_width, text + 2));
			height = icon_height + font + 2;
		}
		break;
	case SCRIPT_LAYOUT_CHECK:
		/* For nonnegative widths this is legacy truncation of width*1.5. */
		width = text * 3 / 2;
		height = font + 5;
		break;
	case SCRIPT_LAYOUT_TEXT:
		width = larger(width, text + 40);
		height = font + 10;
		break;
	case SCRIPT_LAYOUT_HSCROLL:
		width = larger(width, text + input->second_text_width + 20);
		height = font * 2 + 30;
		break;
	case SCRIPT_LAYOUT_VSCROLL:
		width = larger(text, input->second_text_width) * 2 + 30;
		height = larger(height, font * 2 + 30);
		break;
	case SCRIPT_LAYOUT_POPUP:
		width = larger(30, text + 34);
		height = font + 12;
		break;
	case SCRIPT_LAYOUT_MINISCROLL:
		width = 19;
		height = 34;
		break;
	case SCRIPT_LAYOUT_SWALLOW:
		width = larger(width, 30);
		height = larger(height, 30);
		break;
	case SCRIPT_LAYOUT_HDIP:
		width = larger(width, 30);
		height = larger(height, 11);
		break;
	case SCRIPT_LAYOUT_VDIP:
		width = larger(width, 11);
		height = larger(height, 30);
		break;
	case SCRIPT_LAYOUT_LIST:
		width = larger(width, 108);
		height = larger(height, 14 + 3 * larger(1, font + 3));
		break;
	case SCRIPT_LAYOUT_MENU:
		width = text + 6;
		height = font + 4;
		x = input->menu_x;
		y = 2;
		break;
	case SCRIPT_LAYOUT_RECTANGLE:
		break;
	default:
		return false;
	}
	if (!script_geometry_size(width, height) ||
	    !script_geometry_position(x, y))
	{
		return false;
	}
	geometry->width = (int)width;
	geometry->height = (int)height;
	geometry->x = (int)x;
	geometry->y = (int)y;
	return true;
}
