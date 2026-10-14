/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/fd_safety.h"
#include "libs/fsm.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stopped;
static FIceWatchProc watched;
static FIcePointer watch_context;
static int sample_connection, sample_fd;

Status __real_IceAddConnectionWatch(FIceWatchProc, FIcePointer);
int __real_IceConnectionNumber(FIceConn);

Status __wrap_IceAddConnectionWatch(FIceWatchProc callback, FIcePointer data)
{
	watched = callback;
	watch_context = data;
	return __real_IceAddConnectionWatch(callback, data);
}

int __wrap_IceConnectionNumber(FIceConn connection)
{
	if (connection == (FIceConn)&sample_connection)
	{
		return sample_fd;
	}
	return __real_IceConnectionNumber(connection);
}

static void test_poll_snapshot(void)
{
	fd_set expected;
	struct pollfd ready[FD_SETSIZE];
	size_t count;
	int previous = -1;

	FD_ZERO(&expected);
	fsm_fdset(&expected);
	count = fsm_pollfds(ready, FD_SETSIZE);
	assert(count <= FD_SETSIZE);
	assert(fsm_pollfds(NULL, 0) == count);
	for (size_t i = 0; i < count; i++)
	{
		assert(ready[i].fd > previous && ready[i].fd < FD_SETSIZE);
		assert(FD_ISSET(ready[i].fd, &expected));
		assert(ready[i].events == POLLIN && ready[i].revents == 0);
		FD_CLR(ready[i].fd, &expected);
		previous = ready[i].fd;
	}
	for (int fd = 0; fd < FD_SETSIZE; fd++)
	{
		assert(!FD_ISSET(fd, &expected));
	}
	if (count > 0)
	{
		struct pollfd prefix[2] = { { -1, 0, 0 }, { -1, 0, 0 } };

		assert(fsm_pollfds(prefix, 1) == count);
		assert(prefix[0].fd == ready[0].fd);
		assert(prefix[1].fd == -1);
	}
	assert(!fsm_process_pollfds(ready, count));
}

static void test_watch_ranges(void)
{
	static const int invalid[] = { -1, FD_SETSIZE, INT_MAX };
	fd_set expected, actual, empty;

	assert(watched != NULL);
	FD_ZERO(&expected);
	fsm_fdset(&expected);
	test_poll_snapshot();
	for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		FIcePointer view = NULL;

		sample_fd = invalid[i];
		watched((FIceConn)&sample_connection, watch_context, True, &view);
		assert(view != NULL);
		FD_ZERO(&actual);
		fsm_fdset(&actual);
		assert(memcmp(&expected, &actual, sizeof(actual)) == 0);
		FD_ZERO(&empty);
		assert(!fsm_process(&empty));
		test_poll_snapshot();
		watched((FIceConn)&sample_connection, watch_context, False, &view);
		FD_ZERO(&actual);
		fsm_fdset(&actual);
		assert(memcmp(&expected, &actual, sizeof(actual)) == 0);
	}
	puts("session-watch-range-and-borrowed-close: passed");
	fflush(stdout);
}

static void
stop(int signal_number)
{
	(void)signal_number;
	stopped = 1;
}

int
main(int argc, char **argv)
{
	assert(argc == 2);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, stop);
	signal(SIGINT, stop);
	assert(SessionSupport);
	assert(fsm_init("FVWM session probe"));
	test_watch_ranges();
	FILE *endpoint = fopen(argv[1], "w");
	assert(endpoint != NULL);
	assert(fvwm_fd_set_cloexec(fileno(endpoint), true));
	fprintf(endpoint, "%s\n", getenv("SESSION_MANAGER"));
	assert(fclose(endpoint) == 0);
	while (!stopped)
	{
		struct pollfd inputs[FD_SETSIZE];
		size_t count = fsm_pollfds(inputs, FD_SETSIZE);
		int ready = poll(inputs, count, 20);

		assert(ready >= 0 || errno == EINTR);
		if (ready >= 0)
		{
			fsm_process_pollfds(inputs, count);
		}
	}
	fsm_close();
	/* Repeated terminal cleanup must not repeat releases or helper work. */
	fsm_close();
	puts("session-repeated-authentication-cleanup: passed");
	return 0;
}
