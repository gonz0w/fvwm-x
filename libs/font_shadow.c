/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "font_shadow.h"

#include <limits.h>

bool font_shadow_count(int size, int offset, unsigned directions, bool center,
    uint32_t limit, uint32_t *positions)
{
	uint64_t count;
	uint64_t border;
	uint64_t span;

	*positions = 0;
	if (size < 0 || offset < 0 || directions > 8 || limit == 0 ||
	    limit > 65536 || (int64_t)size + offset > INT_MAX)
	{
		return false;
	}
	count = (uint64_t)size * directions;
	if (count > limit)
	{
		return false;
	}
	if (center && size > 0)
	{
		/* Difference of squares: 4*S*(S+2*O+1). Divide before
		 * multiplying so even rejected integer endpoints stay safe. */
		border = 4 * (uint64_t)size;
		span = (uint64_t)size + 2 * (uint64_t)offset + 1;
		if (span > (limit - count) / border)
		{
			return false;
		}
		count += border * span;
	}
	*positions = (uint32_t)count;
	return true;
}

bool font_shadow_center_position(int size, int offset, uint32_t index,
    int *x, int *y)
{
	uint32_t count;
	int64_t outer = (int64_t)size + offset;
	int64_t side = 2 * outer + 1;
	int64_t top;
	int64_t middle;
	int64_t column;
	int64_t step = index;

	*x = *y = 0;
	if (!font_shadow_count(size, offset, 0, true, 65536, &count) ||
	    index >= count)
	{
		return false;
	}
	top = (int64_t)size * side;
	middle = (2 * (int64_t)offset + 1) * 2 * size;
	if (step < top)
	{
		*x = (int)(step % side - outer);
		*y = (int)(step / side - outer);
	}
	else if ((step -= top) < middle)
	{
		column = step % (2 * size);
		*x = (int)(column < size ? column - outer :
		    offset + 1 + column - size);
		*y = (int)(step / (2 * size) - offset);
	}
	else
	{
		step -= middle;
		*x = (int)(step % side - outer);
		*y = (int)(offset + 1 + step / side);
	}
	return true;
}
