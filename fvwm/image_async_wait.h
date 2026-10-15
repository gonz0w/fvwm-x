#ifndef FVWM_IMAGE_ASYNC_WAIT_H
#define FVWM_IMAGE_ASYNC_WAIT_H

#include <stdbool.h>
#include <poll.h>
#include <sys/time.h>

/* Wait on the caller's owned snapshot, including borrowed decoder fds.
 * A decoder-only deadline must not be interpreted as the startup-module
 * timeout. Round fractional milliseconds up; no commands or owner work run
 * here. The caller interprets each descriptor's native poll result. */
int image_async_wait(struct pollfd *descriptors, nfds_t count,
	const struct timeval *timeout, int image_ms, bool *image_timeout);

#endif
