/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "module_x_service.h"

bool module_x_service_available(unsigned int serviced,
	const struct timespec *started, const struct timespec *now)
{
	long elapsed;

	if (serviced >= MODULE_X_EVENT_BUDGET || started->tv_sec < 0 ||
		now->tv_sec < started->tv_sec || started->tv_nsec < 0 ||
		started->tv_nsec >= 1000000000L || now->tv_nsec < 0 ||
		now->tv_nsec >= 1000000000L ||
		(now->tv_sec == started->tv_sec && now->tv_nsec < started->tv_nsec))
	{
		return false;
	}
	/* One event guarantees progress even after scheduler preemption. */
	if (serviced == 0)
	{
		return true;
	}
	if (now->tv_sec == started->tv_sec)
	{
		elapsed = now->tv_nsec - started->tv_nsec;
	}
	else if (now->tv_sec - started->tv_sec == 1 &&
		now->tv_nsec < started->tv_nsec)
	{
		elapsed = 1000000000L - started->tv_nsec + now->tv_nsec;
	}
	else
	{
		return false;
	}
	return elapsed >= 0 && elapsed < MODULE_X_SERVICE_NS;
}

