/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include <limits.h>
#include "menu_delay.h"

bool fvwm_menu_delay_expired(
	int *ticks, uint64_t *previous_ns, uint64_t now_ns, int delay_ticks)
{
	uint64_t elapsed_ticks;
	uint64_t next_ticks;

	if (delay_ticks <= 0)
	{
		return false;
	}
	if (*ticks == 0)
	{
		*ticks = 1;
		*previous_ns = now_ns;
		return false;
	}
	if (now_ns == UINT64_MAX)
	{
		return false;
	}
	if (*previous_ns == UINT64_MAX || now_ns < *previous_ns)
	{
		*previous_ns = now_ns;
		return false;
	}
	elapsed_ticks = (now_ns - *previous_ns) / FVWM_MENU_DELAY_INTERVAL_NS;
	if (elapsed_ticks == 0)
	{
		return false;
	}
	*previous_ns += elapsed_ticks * FVWM_MENU_DELAY_INTERVAL_NS;
	next_ticks = (uint64_t)*ticks + elapsed_ticks;
	if (next_ticks > (uint64_t)delay_ticks + 1)
	{
		*ticks = delay_ticks;
		return true;
	}
	*ticks = next_ticks > INT_MAX ? INT_MAX : (int)next_ticks;
	return false;
}

int fvwm_menu_delay_timeout(uint64_t previous_ns, uint64_t now_ns)
{
	uint64_t remaining_ns;

	if (previous_ns == UINT64_MAX || now_ns == UINT64_MAX ||
	    now_ns < previous_ns)
	{
		return FVWM_MENU_DELAY_INTERVAL_MS;
	}
	if (now_ns - previous_ns >= FVWM_MENU_DELAY_INTERVAL_NS)
	{
		return 0;
	}
	remaining_ns = FVWM_MENU_DELAY_INTERVAL_NS - (now_ns - previous_ns);
	return (int)((remaining_ns + UINT64_C(999999)) / UINT64_C(1000000));
}
