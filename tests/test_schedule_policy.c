/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <assert.h>
#include "fvwm/schedule_policy.h"

static void test_elapsed_deadlines(void)
{
	uint64_t now_ns = 123456789ULL;
	uint64_t deadline_ns = fvwm_schedule_deadline_ns(now_ns, 10);

	assert(fvwm_schedule_timeout_ms(now_ns, deadline_ns) == 10);
	assert(fvwm_schedule_timeout_ms(now_ns + 5000000, deadline_ns) == 5);
	assert(fvwm_schedule_timeout_ms(deadline_ns - 1, deadline_ns) == 1);
	assert(fvwm_schedule_timeout_ms(deadline_ns, deadline_ns) == 0);
	assert(fvwm_schedule_timeout_ms(deadline_ns + 1, deadline_ns) == 0);
	assert(fvwm_schedule_timeout_ms(now_ns,
		fvwm_schedule_deadline_ns(now_ns, 0)) == 0);
}

static void test_limits(void)
{
	assert(fvwm_schedule_deadline_ns(UINT64_MAX - 1, 1) == UINT64_MAX);
	assert(fvwm_schedule_timeout_ms(0, UINT64_MAX) == INT_MAX);
	assert(fvwm_schedule_timeout_ms(0,
		(uint64_t)INT_MAX * 1000000ULL) == INT_MAX);
}

int main(void)
{
	test_elapsed_deadlines();
	test_limits();
	return 0;
}
