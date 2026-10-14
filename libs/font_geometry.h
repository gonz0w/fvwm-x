#ifndef FVWMLIB_FONT_GEOMETRY_H
#define FVWMLIB_FONT_GEOMETRY_H

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

/* Preserve representable metrics. Never let overflow wrap into a small size. */
static inline int font_geometry_int(int64_t value)
{
	return value > INT_MAX ? INT_MAX : value < INT_MIN ? INT_MIN : (int)value;
}

static inline bool font_geometry_shadow(int height, int ascent, int descent,
	int size, int offset, bool upper, bool bottom, bool left, bool right)
{
	int64_t full = (int64_t)size + offset;

	return size >= 0 && offset >= 0 && full <= INT_MAX &&
	    full * (left + right) <= INT_MAX &&
	    (int64_t)height + full * (upper + bottom) <= INT_MAX &&
	    (int64_t)ascent + full * upper <= INT_MAX &&
	    (int64_t)descent + full * bottom <= INT_MAX;
}

#endif
