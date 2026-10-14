#ifndef FVWM_MENU_DELAY_H
#define FVWM_MENU_DELAY_H

#include <stdbool.h>
#include <stdint.h>

#define FVWM_MENU_DELAY_INTERVAL_MS 10
#define FVWM_MENU_DELAY_INTERVAL_NS UINT64_C(10000000)

/* Preserve the existing initial tick and extra delay quantum. Early readiness
 * wakes advance only whole monotonic ticks, keeping the fractional remainder.
 * A zero counter starts a new delay. Invalid clocks keep waits finite. */
bool fvwm_menu_delay_expired(
	int *ticks, uint64_t *previous_ns, uint64_t now_ns, int delay_ticks);
int fvwm_menu_delay_timeout(uint64_t previous_ns, uint64_t now_ns);

#endif
