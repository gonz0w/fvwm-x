#ifndef FVWM_SCHEDULE_POLICY_H
#define FVWM_SCHEDULE_POLICY_H

#include <limits.h>
#include <stdint.h>

/* Deadlines use elapsed time, independently of X event timestamps. */
static inline uint64_t fvwm_schedule_deadline_ns(
	uint64_t now_ns, unsigned int delay_ms)
{
	uint64_t delay_ns = (uint64_t)delay_ms * 1000000ULL;

	return delay_ns > UINT64_MAX - now_ns ? UINT64_MAX :
		now_ns + delay_ns;
}

static inline int fvwm_schedule_timeout_ms(
	uint64_t now_ns, uint64_t deadline_ns)
{
	uint64_t remaining_ns;
	uint64_t remaining_ms;

	if (now_ns >= deadline_ns)
	{
		return 0;
	}
	remaining_ns = deadline_ns - now_ns;
	/* Round up so descriptor polling cannot run a command early. */
	remaining_ms = remaining_ns / 1000000ULL +
		(remaining_ns % 1000000ULL != 0);
	return remaining_ms > INT_MAX ? INT_MAX : (int)remaining_ms;
}

#endif
