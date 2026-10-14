#ifndef FVWM_IMAGE_ASYNC_WAIT_H
#define FVWM_IMAGE_ASYNC_WAIT_H

#include <stdbool.h>
#include <poll.h>
#include <sys/select.h>

/* Base descriptors retain the existing select contract. The two borrowed
 * decoder descriptors may exceed FD_SETSIZE. A decoder-only deadline must not
 * be interpreted as the startup-module timeout. No owner work runs here. */
int image_async_wait(int limit, fd_set *reads, fd_set *writes,
	const struct pollfd extra[2], struct timeval *timeout, int image_ms,
	bool *image_timeout);

#endif
