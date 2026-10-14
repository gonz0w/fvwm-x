/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "script_widget_math.h"
#include <limits.h>
#include <stdint.h>

_Static_assert(INT_MAX <= INT32_MAX, "Widget integers need at most 32 bits");

int script_widget_initial_maximum(int minimum, int maximum, int increment,
	bool include_equal)
{
	int64_t expanded;

	if (maximum > minimum || (maximum == minimum && !include_equal))
	{
		return maximum;
	}
	expanded = (int64_t)minimum + increment;
	return expanded > INT_MAX ? INT_MAX :
		expanded < INT_MIN ? INT_MIN : (int)expanded;
}

int script_widget_position(int value, int minimum, int maximum, int extent,
	int inset, bool reverse)
{
	int64_t span = (int64_t)extent - inset;
	int64_t range = (int64_t)maximum - minimum;
	int64_t offset;

	if (span <= 0 || span > INT_MAX || range <= 0)
	{
		return 0;
	}
	if (value < minimum)
	{
		value = minimum;
	}
	else if (value > maximum)
	{
		value = maximum;
	}
	offset = (int64_t)value - minimum;
	if (reverse)
	{
		offset = range - offset;
	}
	/* Positive int32 span times an unsigned-int32 range fits int64. */
	return (int)(span * offset / range);
}

int script_widget_pointer_position(int position, int origin, int extent)
{
	int64_t relative = (int64_t)position - origin;

	if (extent <= 36 || relative < 15)
	{
		return 15;
	}
	if (relative > (int64_t)extent - 21)
	{
		return extent - 21;
	}
	return (int)relative;
}

int script_widget_pointer_value(int position, int extent, int minimum,
	int maximum, bool reverse)
{
	int64_t pixel, scaled, start, difference;

	if (extent <= 36 || maximum <= minimum)
	{
		return minimum;
	}
	pixel = script_widget_pointer_position(position, 0, extent) - 15;
	scaled = pixel * extent / (extent - 36);
	start = reverse ? maximum : minimum;
	difference = reverse ? (int64_t)minimum - maximum :
		(int64_t)maximum - minimum;
	return (int)(scaled * difference / extent + start);
}

int script_widget_step(int value, int minimum, int maximum, bool forward)
{
	int64_t next = (int64_t)value + (forward ? 1 : -1);

	if (forward && (next > maximum || next > INT_MAX))
	{
		return minimum;
	}
	if (!forward && (next < minimum || next < INT_MIN))
	{
		return maximum;
	}
	return (int)next;
}

int script_widget_dimension(int64_t value)
{
	return value < 0 ? 0 : value > UINT16_MAX ? UINT16_MAX : (int)value;
}

int script_list_row_height(int font_height)
{
	int64_t height = (int64_t)font_height + 3;
	return height < 1 ? 1 : height > INT_MAX ? INT_MAX : (int)height;
}

int script_list_first(int first, int count, int visible)
{
	int maximum;

	if (visible < 1)
	{
		visible = 1;
	}
	maximum = count > visible ? count - visible + 1 : 1;
	return first < 1 ? 1 : first > maximum ? maximum : first;
}

void script_list_thumb(int first, int count, int visible, int track,
	int *position, int *size)
{
	int64_t span;

	*position = 0;
	*size = track < 0 ? 0 : track;
	if (track <= 8 || count <= 0 || visible < 1)
	{
		return;
	}
	span = (int64_t)track - 8;
	first = script_list_first(first, count, visible);
	if (visible < count)
	{
		*size = (int)((int64_t)visible * span / count) + 8;
	}
	*position = (int)((int64_t)(first - 1) * span / count);
}

int script_list_pointer_first(int pointer, int origin, int track, int count,
	int visible)
{
	int64_t first;

	if (track <= 0 || count <= 0)
	{
		return 1;
	}
	first = ((int64_t)pointer - origin) * count / track;
	if (first < 1)
	{
		first = 1;
	}
	else if (first > INT_MAX)
	{
		first = INT_MAX;
	}
	return script_list_first((int)first, count, visible);
}

int script_list_move(int first, int delta, int count, int visible)
{
	int64_t next = (int64_t)first + delta;

	return script_list_first(next < 1 ? 1 : next > INT_MAX ? INT_MAX :
		(int)next, count, visible);
}

int script_list_cell(int first, int row, int count)
{
	int64_t cell = (int64_t)first + row;

	return cell < 1 || cell > count ? 0 : (int)cell;
}
