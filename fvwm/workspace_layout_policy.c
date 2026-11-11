/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"
#include "workspace_layout_policy.h"
#include <limits.h>

static void partition(rectangle area, int count, bool columns,
	rectangle *cells)
{
	int extent = columns ? area.width : area.height;
	int origin = columns ? area.x : area.y;
	int base = extent / count;
	int remainder = extent % count;

	for (int i = 0; i < count; i++)
	{
		int size = base + (i < remainder);

		cells[i] = area;
		if (columns)
		{
			cells[i].x = origin;
			cells[i].width = size;
		}
		else
		{
			cells[i].y = origin;
			cells[i].height = size;
		}
		origin += size;
	}
}

bool workspace_layout_plan(enum workspace_layout_kind kind, rectangle area,
	size_t count, struct workspace_layout_plan *out)
{
	int cells;

	if (!out)
		return false;
	*out = (struct workspace_layout_plan){0};
	/* All generated offsets end at one of these representable edges. */
	if (count == 0 || count > WORKSPACE_LAYOUT_MAX ||
	    area.width <= 0 || area.height <= 0 ||
	    area.x > INT_MAX - area.width ||
	    area.y > INT_MAX - area.height)
		return false;
	cells = (int)count;
	switch (kind)
	{
	case WORKSPACE_LAYOUT_COLUMNS:
		if (cells > area.width)
			return false;
		partition(area, cells, true, out->cells);
		break;
	case WORKSPACE_LAYOUT_MASTER_STACK:
		if (cells == 1)
		{
			out->cells[0] = area;
			break;
		}
		if (area.width < 2 || cells - 1 > area.height)
			return false;
		out->cells[0] = area;
		out->cells[0].width = area.width - area.width / 2;
		area.x += out->cells[0].width;
		area.width /= 2;
		partition(area, cells - 1, false, out->cells + 1);
		break;
	default:
		return false;
	}
	out->count = count;
	return true;
}
