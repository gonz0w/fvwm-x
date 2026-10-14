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

int image_async_wait(int limit, fd_set *reads, fd_set *writes,
	const struct pollfd extra[2], struct timeval *timeout, int image_ms,
	bool *image_timeout)
{
	struct timeval image_time;
	struct timeval *selected = timeout;
	struct pollfd descriptors[FD_SETSIZE + 2];
	nfds_t count = 0;

	*image_timeout = false;
	if (limit < 0 || limit > FD_SETSIZE || image_ms < -1 ||
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
	if (extra[0].fd < 0 && extra[1].fd < 0)
	{
		return fvwmSelect(limit, reads, writes, NULL, selected);
	}
	for (int fd = 0; fd < limit; fd++)
	{
		short events = (FD_ISSET(fd, reads) ? POLLIN : 0) |
		    (FD_ISSET(fd, writes) ? POLLOUT : 0);

		if (events != 0)
		{
			descriptors[count++] = (struct pollfd){fd, events, 0};
		}
	}
	nfds_t base_count = count;

	descriptors[count++] = extra[0];
	descriptors[count++] = extra[1];
	int timeout_ms = -1;

	if (selected != NULL)
	{
		long fraction_ms = (selected->tv_usec + 999) / 1000;

		timeout_ms = selected->tv_sec > (INT_MAX - fraction_ms) / 1000 ?
		    INT_MAX : (int)(selected->tv_sec * 1000 + fraction_ms);
	}
	int result = fvwmPoll(descriptors, count, timeout_ms);

	if (result < 0)
	{
		return result;
	}
	for (nfds_t i = 0; i < base_count; i++)
	{
		if (descriptors[i].revents & POLLNVAL)
		{
			errno = EBADF;
			return -1;
		}
	}
	FD_ZERO(reads);
	FD_ZERO(writes);
	for (nfds_t i = 0; i < base_count; i++)
	{
		struct pollfd *fd = &descriptors[i];

		if ((fd->events & POLLIN) &&
		    (fd->revents & (POLLIN | POLLHUP | POLLERR)))
		{
			FD_SET(fd->fd, reads);
		}
		if ((fd->events & POLLOUT) &&
		    (fd->revents & (POLLOUT | POLLHUP | POLLERR)))
		{
			FD_SET(fd->fd, writes);
		}
	}
	/* Include extra readiness in the result even when no base fd is ready.
	 * The caller services transport and yields to its outer boundary. */
	return result;
}
