/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "script_delay.h"
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>

_Static_assert(INT_MAX <= INT32_MAX, "widget delay requires 32-bit int");

void script_delay_duration(int ticks, struct timespec *duration)
{
	int64_t microseconds = ticks > 0 ? (int64_t)ticks * 16667 : 0;

	duration->tv_sec = (time_t)(microseconds / 1000000);
	duration->tv_nsec = (long)(microseconds % 1000000) * 1000;
}

static int script_interval_timeout_ms(const struct timespec *last,
	const struct timespec *now, int interval)
{
	uintmax_t seconds;
	int64_t remaining;

	if (now->tv_sec < last->tv_sec ||
		(now->tv_sec == last->tv_sec && now->tv_nsec < last->tv_nsec))
	{
		return interval * 1000;
	}
	/* Unsigned subtraction also covers distant signed time_t endpoints. */
	seconds = (uintmax_t)now->tv_sec - (uintmax_t)last->tv_sec;
	if (seconds > (unsigned int)interval)
	{
		return 0;
	}
	remaining = (interval - (int64_t)seconds) * 1000000000 +
		last->tv_nsec - now->tv_nsec;
	return remaining > 0 ? (int)((remaining + 999999) / 1000000) : 0;
}

int script_periodic_timeout_ms(const struct timespec *last,
	const struct timespec *now)
{
	return script_interval_timeout_ms(last, now, 1);
}

int script_shutdown_timeout_ms(const struct timespec *start,
	const struct timespec *now)
{
	return script_interval_timeout_ms(start, now, 2);
}

int script_readiness_timeout_ms(bool periodic, bool session_pending,
	const struct timespec *last, const struct timespec *now)
{
	int timeout = periodic ? script_periodic_timeout_ms(last, now) : -1;

	if (session_pending && (timeout < 0 || timeout > 10))
	{
		return 10;
	}
	return timeout;
}

script_delay_result script_delay_wait(int ticks, int fd,
	script_delay_cancel_fn check, void *context)
{
	struct timespec duration, start, now, elapsed, remaining;
	struct pollfd input = {.fd = fd, .events = POLLIN};

	if (fd < -1 || (fd >= 0 && check == NULL))
	{
		errno = EINVAL;
		return SCRIPT_DELAY_ERROR;
	}
	if (check != NULL && check(context))
	{
		return SCRIPT_DELAY_CANCELLED;
	}
	script_delay_duration(ticks, &duration);
	if (ticks <= 0)
	{
		return SCRIPT_DELAY_ELAPSED;
	}
	if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
	{
		return SCRIPT_DELAY_ERROR;
	}
	for (;;)
	{
		if (check != NULL && check(context))
		{
			return SCRIPT_DELAY_CANCELLED;
		}
		if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
		{
			return SCRIPT_DELAY_ERROR;
		}
		elapsed.tv_sec = now.tv_sec - start.tv_sec;
		elapsed.tv_nsec = now.tv_nsec - start.tv_nsec;
		if (elapsed.tv_nsec < 0)
		{
			elapsed.tv_sec--;
			elapsed.tv_nsec += 1000000000;
		}
		if (elapsed.tv_sec > duration.tv_sec ||
		    (elapsed.tv_sec == duration.tv_sec &&
		     elapsed.tv_nsec >= duration.tv_nsec))
		{
			return SCRIPT_DELAY_ELAPSED;
		}
		remaining.tv_sec = duration.tv_sec - elapsed.tv_sec;
		remaining.tv_nsec = duration.tv_nsec - elapsed.tv_nsec;
		if (remaining.tv_nsec < 0)
		{
			remaining.tv_sec--;
			remaining.tv_nsec += 1000000000;
		}
		/* Recheck elapsed time after waiting. */
		if (fd < 0 && check == NULL)
		{
			if (nanosleep(&remaining, NULL) < 0 && errno != EINTR)
			{
				return SCRIPT_DELAY_ERROR;
			}
		}
		else
		{
			/* Round up without shortening a tick. */
			int64_t timeout = (int64_t)remaining.tv_sec * 1000 +
				(remaining.tv_nsec + 999999) / 1000000;
			/* A signal can arrive just before poll.
			 * Periodic checks bound that race without spinning. */
			if (timeout > 100)
			{
				timeout = 100;
			}
			int result = poll(&input, 1, (int)timeout);

			if (result < 0 && errno != EINTR)
			{
				return SCRIPT_DELAY_ERROR;
			}
			if (result > 0 &&
			    (input.revents & (POLLERR | POLLHUP | POLLNVAL)))
			{
				errno = input.revents & POLLNVAL ? EBADF : EIO;
				return SCRIPT_DELAY_ERROR;
			}
		}
	}
}

bool script_delay_ticks(int ticks)
{
	return script_delay_wait(ticks, -1, NULL, NULL) == SCRIPT_DELAY_ELAPSED;
}
