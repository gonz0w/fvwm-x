/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <limits.h>
#include "fvwm/menu_delay.h"

static void test_early_wakes_and_fractional_ticks(void)
{
	int ticks = 0;
	uint64_t origin = UINT64_C(123456789);
	uint64_t previous = 0;

	assert(!fvwm_menu_delay_expired(&ticks, &previous, origin, 2));
	assert(ticks == 1 && previous == origin);
	for (int i = 0; i < 1000; i++)
	{
		assert(!fvwm_menu_delay_expired(
			&ticks, &previous, origin + i, 2));
	}
	assert(ticks == 1 && previous == origin);
	assert(!fvwm_menu_delay_expired(&ticks, &previous,
		origin + FVWM_MENU_DELAY_INTERVAL_NS + 1, 2));
	assert(ticks == 2 && previous == origin + FVWM_MENU_DELAY_INTERVAL_NS);
	assert(!fvwm_menu_delay_expired(&ticks, &previous,
		origin + 3 * FVWM_MENU_DELAY_INTERVAL_NS - 1, 2));
	assert(ticks == 3);
	assert(fvwm_menu_delay_expired(&ticks, &previous,
		origin + 3 * FVWM_MENU_DELAY_INTERVAL_NS, 2));
	assert(ticks == 2);
}

static void test_reset_disabled_clock_and_large_delays(void)
{
	int ticks = 0;
	uint64_t previous = 0;

	assert(!fvwm_menu_delay_expired(&ticks, &previous, 10, 0));
	assert(!fvwm_menu_delay_expired(&ticks, &previous, 10, -1));
	assert(ticks == 0);
	assert(!fvwm_menu_delay_expired(&ticks, &previous, UINT64_MAX, 1));
	assert(!fvwm_menu_delay_expired(&ticks, &previous, 100, 1));
	assert(previous == 100 && ticks == 1);
	assert(!fvwm_menu_delay_expired(&ticks, &previous, 99, 1));
	assert(previous == 99 && ticks == 1);
	assert(!fvwm_menu_delay_expired(&ticks, &previous, UINT64_MAX, 1));
	assert(fvwm_menu_delay_expired(&ticks, &previous,
		99 + 2 * FVWM_MENU_DELAY_INTERVAL_NS, 1));
	ticks = 0;
	assert(!fvwm_menu_delay_expired(&ticks, &previous, 100, INT_MAX));
	assert(fvwm_menu_delay_expired(&ticks, &previous,
		100 + ((uint64_t)INT_MAX + 1) * FVWM_MENU_DELAY_INTERVAL_NS,
		INT_MAX));
	assert(ticks == INT_MAX);
	ticks = 0;
	assert(!fvwm_menu_delay_expired(&ticks, &previous, UINT64_MAX - 10, 1));
	assert(!fvwm_menu_delay_expired(&ticks, &previous, UINT64_MAX - 1, 1));
}

static void test_finite_readiness(void)
{
	assert(fvwm_menu_delay_timeout(100, 100) == 10);
	assert(fvwm_menu_delay_timeout(100, 101) == 10);
	assert(fvwm_menu_delay_timeout(100, 100 + UINT64_C(1000000)) == 9);
	assert(fvwm_menu_delay_timeout(100,
		100 + FVWM_MENU_DELAY_INTERVAL_NS - 1) == 1);
	assert(fvwm_menu_delay_timeout(100,
		100 + FVWM_MENU_DELAY_INTERVAL_NS) == 0);
	assert(fvwm_menu_delay_timeout(100, UINT64_MAX - 1) == 0);
	assert(fvwm_menu_delay_timeout(100, 99) == 10);
	assert(fvwm_menu_delay_timeout(100, UINT64_MAX) == 10);
	assert(fvwm_menu_delay_timeout(UINT64_MAX, 100) == 10);
}

int main(void)
{
	test_early_wakes_and_fractional_ticks();
	test_reset_disabled_clock_and_large_delays();
	test_finite_readiness();
	return 0;
}
