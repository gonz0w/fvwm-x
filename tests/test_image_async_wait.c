/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/image_async_wait.h"
#include "libs/fvwmsignal.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>

static unsigned poll_calls;
static unsigned select_calls;
static int recorded_timeout;
static bool fake_timeout;
static bool terminate_in_poll;
static bool synthetic_high_fd;

int __real_poll(struct pollfd *, nfds_t, int);
int __real_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);

int __wrap_poll(struct pollfd *fds, nfds_t count, int timeout)
{
	poll_calls++;
	recorded_timeout = timeout;
	if (synthetic_high_fd)
	{
		bool found = false;

		for (nfds_t i = 0; i < count; i++)
		{
			fds[i].revents = fds[i].fd > FD_SETSIZE ? POLLIN : 0;
			found = found || fds[i].revents != 0;
		}
		assert(found);
		return 1;
	}
	if (terminate_in_poll)
	{
		fvwmSetTerminate(SIGTERM);
		assert(!"termination must return to the wrapper");
	}
	if (fake_timeout)
	{
		for (nfds_t i = 0; i < count; i++)
		{
			fds[i].revents = 0;
		}
		return 0;
	}
	return __real_poll(fds, count, timeout);
}

int __wrap_select(int limit, fd_set *reads, fd_set *writes,
	fd_set *errors, struct timeval *timeout)
{
	select_calls++;
	return __real_select(limit, reads, writes, errors, timeout);
}

static void test_no_worker(void)
{
	fd_set reads, writes;
	struct pollfd extra[2] = {{-1, 0, 0}, {-1, 0, 0}};
	struct timeval timeout = {0, 0};
	bool image_timeout = true;

	FD_ZERO(&reads);
	FD_ZERO(&writes);
	assert(image_async_wait(0, &reads, &writes, extra, &timeout, -1,
	    &image_timeout) == 0);
	assert(!image_timeout && select_calls == 1 && poll_calls == 0);
	assert(image_async_wait(0, &reads, &writes, extra, NULL, 0,
	    &image_timeout) == 0 && image_timeout);
	assert(select_calls == 2 && poll_calls == 0);
}

static void test_readiness_and_high_fd(void)
{
	int base[2], worker[2];
	fd_set reads, writes;
	bool image_timeout;
	struct timeval timeout = {0, 0};
	struct rlimit saved, raised;

	assert(pipe(base) == 0 && pipe(worker) == 0);
	assert(getrlimit(RLIMIT_NOFILE, &saved) == 0);
	raised = saved;
	if (raised.rlim_cur <= FD_SETSIZE &&
	    raised.rlim_max > FD_SETSIZE + 3)
	{
		raised.rlim_cur = FD_SETSIZE + 4;
		assert(setrlimit(RLIMIT_NOFILE, &raised) == 0);
	}
	/* Some BSD login classes impose a hard limit below FD_SETSIZE. Exercise
	 * real pipe readiness there too, then test the high-fd poll contract
	 * separately without changing the administrator's hard limit. */
	int high_fd = fcntl(worker[0], F_DUPFD,
	    raised.rlim_cur > FD_SETSIZE ? FD_SETSIZE : 0);

	assert(high_fd >= 0);
	if (raised.rlim_cur > FD_SETSIZE)
	{
		assert(high_fd >= FD_SETSIZE);
	}
	struct pollfd extra[2] = {{high_fd, POLLIN, 0}, {-1, 0, 0}};

	assert(write(worker[1], "w", 1) == 1);
	FD_ZERO(&reads);
	FD_ZERO(&writes);
	FD_SET(base[0], &reads);
	assert(image_async_wait(base[1] + 1, &reads, &writes, extra,
	    &timeout, -1, &image_timeout) == 1);
	assert(!FD_ISSET(base[0], &reads) && !image_timeout);
	assert(write(base[1], "b", 1) == 1);
	FD_SET(base[0], &reads);
	FD_SET(base[1], &writes);
	assert(image_async_wait(base[1] + 1, &reads, &writes, extra,
	    &timeout, -1, &image_timeout) == 3);
	assert(FD_ISSET(base[0], &reads) && FD_ISSET(base[1], &writes));
	char byte;

	assert(read(base[0], &byte, 1) == 1 && byte == 'b');
	assert(read(high_fd, &byte, 1) == 1 && byte == 'w');
	assert(close(base[1]) == 0);
	FD_ZERO(&writes);
	assert(image_async_wait(base[0] + 1, &reads, &writes, extra,
	    &timeout, -1, &image_timeout) == 1);
	assert(FD_ISSET(base[0], &reads)); /* EOF remains readable. */
	assert(close(base[0]) == 0);
	assert(image_async_wait(base[0] + 1, &reads, &writes, extra,
	    &timeout, -1, &image_timeout) == -1 && errno == EBADF);
	assert(FD_ISSET(base[0], &reads)); /* Error preserves input sets. */
	FD_ZERO(&reads);
	assert(close(high_fd) == 0);
	assert(image_async_wait(0, &reads, &writes, extra, &timeout, -1,
	    &image_timeout) == 1); /* Worker POLLNVAL yields to transport. */
	assert(close(worker[0]) == 0 && close(worker[1]) == 0);
	assert(setrlimit(RLIMIT_NOFILE, &saved) == 0);
	extra[0].fd = FD_SETSIZE + 1;
	synthetic_high_fd = true;
	assert(image_async_wait(0, &reads, &writes, extra, &timeout, -1,
	    &image_timeout) == 1);
	synthetic_high_fd = false;
}

static void test_deadlines(void)
{
	fd_set reads, writes;
	struct pollfd extra[2] = {{0, POLLIN, 0}, {-1, 0, 0}};
	struct timeval timeout = {42, 0};
	bool image_timeout;

	FD_ZERO(&reads);
	FD_ZERO(&writes);
	fake_timeout = true;
	assert(image_async_wait(0, &reads, &writes, extra, &timeout, 5,
	    &image_timeout) == 0 && image_timeout);
	assert(recorded_timeout == 5 && timeout.tv_sec == 42);
	timeout = (struct timeval){0, 5000};
	assert(image_async_wait(0, &reads, &writes, extra, &timeout, 5,
	    &image_timeout) == 0 && !image_timeout);
	timeout.tv_usec = 4999;
	assert(image_async_wait(0, &reads, &writes, extra, &timeout, 5,
	    &image_timeout) == 0 && !image_timeout);
	assert(recorded_timeout == 5); /* Round up; do not busy-spin. */
	assert(image_async_wait(0, &reads, &writes, extra, NULL, -1,
	    &image_timeout) == 0 && !image_timeout);
	assert(recorded_timeout == -1);
	timeout = (struct timeval){INT_MAX, 999999};
	assert(image_async_wait(0, &reads, &writes, extra, &timeout, -1,
	    &image_timeout) == 0 && recorded_timeout == INT_MAX);
	timeout.tv_usec = 1000000;
	assert(image_async_wait(0, &reads, &writes, extra, &timeout, 0,
	    &image_timeout) == -1 && errno == EINVAL);
	assert(image_async_wait(FD_SETSIZE + 1, &reads, &writes, extra,
	    NULL, -1, &image_timeout) == -1 && errno == EINVAL);
	assert(image_async_wait(0, &reads, &writes, extra, NULL, -2,
	    &image_timeout) == -1 && errno == EINVAL);
	fake_timeout = false;
}

static void test_termination(void)
{
	unsigned before = poll_calls;

	isTerminated = 1;
	assert(fvwmPoll(NULL, 0, -1) == -1 && errno == EINTR);
	assert(poll_calls == before);
	isTerminated = 0;
	terminate_in_poll = true;
	assert(fvwmPoll(NULL, 0, -1) == -1 && errno == EINTR);
	assert(isTerminated && poll_calls == before + 1);
	isTerminated = 0;
	terminate_in_poll = false;
	assert(fvwmPoll(NULL, 0, 0) == 0);
}

int main(void)
{
	alarm(10);
	test_no_worker();
	test_readiness_and_high_fd();
	test_deadlines();
	test_termination();
	puts("image readiness preserves base I/O, deadlines and termination");
	return 0;
}
