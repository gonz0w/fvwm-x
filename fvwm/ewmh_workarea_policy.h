#ifndef FVWM_EWMH_WORKAREA_POLICY_H
#define FVWM_EWMH_WORKAREA_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Root-edge distances are CARDINALs, not signed monitor-local offsets. */
static inline bool fvwm_ewmh_strut_valid(const uint32_t values[4],
	int width, int height)
{
	return values != NULL && width > 0 && height > 0 &&
		(uint64_t)values[0] + values[1] <= (unsigned)width &&
		(uint64_t)values[2] + values[3] <= (unsigned)height;
}

/* Independent valid reservations can overlap. Publish a contained empty
 * rectangle in that case, without unsigned subtraction or negative sizes. */
static inline void fvwm_ewmh_workarea(unsigned width, unsigned height,
	const uint32_t edges[4], long out[4])
{
	unsigned left = edges[0] < width ? edges[0] : width;
	unsigned right = edges[1] < width - left ? edges[1] : width - left;
	unsigned top = edges[2] < height ? edges[2] : height;
	unsigned bottom = edges[3] < height - top ? edges[3] : height - top;

	out[0] = left;
	out[1] = top;
	out[2] = width - left - right;
	out[3] = height - top - bottom;
}

#endif
