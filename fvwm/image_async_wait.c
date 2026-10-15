/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "image_async_wait.h"
#include "libs/fvwmsignal.h"

#include <errno.h>
#include <limits.h>

int image_async_wait(struct pollfd *descriptors, nfds_t count,
	const struct timeval *timeout, int image_ms, bool *image_timeout)
{
	struct timeval image_time;
	const struct timeval *selected = timeout;
	int timeout_ms = -1;

	*image_timeout = false;
	if ((count != 0 && descriptors == NULL) || image_ms < -1 ||
	    (timeout != NULL && (timeout->tv_sec < 0 ||
	    timeout->tv_usec < 0 || timeout->tv_usec >= 1000000)))
	{
		errno = EINVAL;
		return -1;
	}
	if (image_ms >= 0)
	{
		image_time.tv_sec = image_ms / 1000;
		image_time.tv_usec = (image_ms % 1000) * 1000;
		if (timeout == NULL || image_time.tv_sec < timeout->tv_sec ||
		    (image_time.tv_sec == timeout->tv_sec &&
		    image_time.tv_usec < timeout->tv_usec))
		{
			selected = &image_time;
			*image_timeout = true;
		}
	}
	if (selected != NULL)
	{
		long fraction_ms = (selected->tv_usec + 999) / 1000;

		timeout_ms = selected->tv_sec > (INT_MAX - fraction_ms) / 1000 ?
		    INT_MAX : (int)(selected->tv_sec * 1000 + fraction_ms);
	}
	return fvwmPoll(descriptors, count, timeout_ms);
}
