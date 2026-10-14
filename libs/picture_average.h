#ifndef FVWMLIB_PICTURE_AVERAGE_H
#define FVWMLIB_PICTURE_AVERAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	uint64_t red;
	uint64_t green;
	uint64_t blue;
	uint64_t count;
} picture_average;

/* Zero-initialize once, then add decoded straight-ARGB pixels during bounded
 * conversion. Fully transparent pixels contribute nothing. Refusal preserves
 * the accumulator. The bound reserves space for conversion to 16-bit RGB. */
bool picture_average_add(picture_average *average, uint32_t pixel);
/* Clear outputs on empty/invalid input. Scale sums before integer division,
 * preserving fractional 8-bit means in the 16-bit color range. */
bool picture_average_finish(const picture_average *average, uint16_t rgb[3]);

#endif
