/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdint.h>
#include "fvwm/stack_rank_policy.h"

static void test_endpoints_and_cancellation(void)
{
	assert(fvwm_stack_rank_add_parent(0, INT64_MAX) == INT64_MAX);
	assert(fvwm_stack_rank_add_parent(1, INT64_MAX) == INT64_MAX);
	assert(fvwm_stack_rank_add_parent(-1, INT64_MAX) == INT64_MAX);
	assert(fvwm_stack_rank_add_parent(INT64_MIN, INT64_MAX) == 0);
	assert(fvwm_stack_rank_add_parent(INT64_MAX, INT64_MIN) == 0);
	assert(fvwm_stack_rank_add_parent(INT64_MIN, -1) == INT64_MIN);
	assert(fvwm_stack_rank_add_parent(INT64_MIN, -2) == INT64_MIN);
	assert(fvwm_stack_rank_add_parent(INT64_MAX, -2) == INT64_MAX - 1);
	assert(fvwm_stack_rank_add_parent(-1, INT64_MIN) == INT64_MIN);
}

static void test_representable_weights(void)
{
	assert(fvwm_stack_rank_add_parent(200000, 2147410737) == 2147610738);
	assert(fvwm_stack_rank_add_parent(-2000000000, -23995999988001LL) ==
		-23997999988000LL);
	assert(fvwm_stack_rank_add_parent(0, 0) == 1);
	assert(fvwm_stack_rank_add_parent(200000, -2000000000) == -1999799999);
}

static void test_boundary_cross_product(void)
{
#if defined(__SIZEOF_INT128__)
	const int64_t values[] = { INT64_MIN, INT64_MIN + 1, -2000000000,
		-200000, -1, 0, 1, 200000, INT64_MAX - 1, INT64_MAX };

	/* An independent wider oracle checks cancellation as well as overflow. */
	for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); i++)
	{
		for (unsigned j = 0; j < sizeof(values) / sizeof(values[0]); j++)
		{
			__int128 expected = (__int128)values[i] + values[j] + 1;

			if (expected > INT64_MAX)
			{
				expected = INT64_MAX;
			}
			if (expected < INT64_MIN)
			{
				expected = INT64_MIN;
			}
			assert(fvwm_stack_rank_add_parent(values[i], values[j]) ==
				(int64_t)expected);
		}
	}
#endif
}

int main(void)
{
	test_endpoints_and_cancellation();
	test_representable_weights();
	test_boundary_cross_product();
	return 0;
}
