/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include "fvwm/ewmh_workarea_policy.h"

int main(void)
{
	uint32_t edges[4] = { 10, 20, 30, 40 };
	long out[4];

	assert(fvwm_ewmh_strut_valid(edges, 800, 600));
	fvwm_ewmh_workarea(800, 600, edges, out);
	assert(out[0] == 10 && out[1] == 30 &&
		out[2] == 770 && out[3] == 530);
	assert(!fvwm_ewmh_strut_valid(NULL, 800, 600));
	assert(!fvwm_ewmh_strut_valid(edges, 0, 600));
	assert(!fvwm_ewmh_strut_valid(edges, 800, -1));
	for (unsigned i = 0; i < 4; i++)
	{
		uint32_t saved = edges[i];

		edges[i] = UINT32_MAX;
		assert(!fvwm_ewmh_strut_valid(edges, INT_MAX, INT_MAX));
		edges[i] = saved;
	}
	edges[0] = edges[1] = INT_MAX;
	assert(!fvwm_ewmh_strut_valid(edges, INT_MAX, INT_MAX));
	fvwm_ewmh_workarea(800, 600, edges, out);
	assert(out[0] == 800 && out[2] == 0);
	/* Independently check containment and exact ordinary subtraction over
	 * every pair of opposing reservations on a small viewport. */
	for (unsigned left = 0; left <= 65; left++)
		for (unsigned right = 0; right <= 65; right++)
		{
			edges[0] = left;
			edges[1] = right;
			edges[2] = right;
			edges[3] = left;
			assert(fvwm_ewmh_strut_valid(edges, 64, 64) ==
				(left + right <= 64));
			fvwm_ewmh_workarea(64, 64, edges, out);
			assert(out[0] >= 0 && out[1] >= 0 &&
				out[2] >= 0 && out[3] >= 0);
			assert(out[0] + out[2] <= 64 &&
				out[1] + out[3] <= 64);
			if (left + right <= 64)
				assert(out[2] == 64 - left - right &&
					out[3] == 64 - left - right);
		}
	return 0;
}
