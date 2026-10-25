/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/fvwmsignal.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static volatile sig_atomic_t timer_signal;
static volatile sig_atomic_t unrelated_signals;

static void terminate_handler(int sig)
{
	fvwmSetTerminate(sig);
}

static void unrelated_handler(int sig)
{
	(void)sig;
	unrelated_signals++;
}

static void timer_handler(int sig)
{
	(void)sig;
	/* Deliver the real installed handler while a wrapper is blocked. */
	if (kill(getpid(), timer_signal) != 0)
	{
		_exit(2);
	}
}

static void arm_timer(int sig)
{
	const struct itimerval timer = {{0, 0}, {0, 30000}};

	timer_signal = sig;
	assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
}

static void assert_mask(const sigset_t *expected)
{
	sigset_t actual;

	assert(sigprocmask(SIG_SETMASK, NULL, &actual) == 0);
	for (int sig = 1; sig < NSIG; sig++)
	{
		if (sigismember(&actual, sig) != sigismember(expected, sig))
		{
			fprintf(stderr, "caller mask changed for signal %d\n", sig);
			assert(!"wrapper must restore the exact caller mask");
		}
	}
}

static void assert_disarmed(const sigset_t *expected)
{
	/* A jump into a returned wrapper would abandon this handler/caller. */
	assert(raise(SIGTERM) == 0);
	assert(isTerminated);
	assert_mask(expected);
}

static void reset_for_test(void)
{
	/* Production termination is permanent. Only the fixture reuses it. */
	isTerminated = 0;
}

static void set_nonblocking(int fd, int enabled)
{
	int flags = fcntl(fd, F_GETFL);

	assert(flags >= 0);
	assert(fcntl(fd, F_SETFL,
	    enabled ? flags | O_NONBLOCK : flags & ~O_NONBLOCK) == 0);
}

static void fill_pipe(int fd)
{
	char buffer[4096] = {0};
	size_t total = 0;
	ssize_t count;

	set_nonblocking(fd, 1);
	while ((count = write(fd, buffer, sizeof(buffer))) > 0)
	{
		total += (size_t)count;
		assert(total < 16 * 1024 * 1024);
	}
	assert(count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
	/* A short free tail must not let the one-byte blocking write succeed. */
	while ((count = write(fd, buffer, 1)) > 0)
	{
		total++;
		assert(total < 16 * 1024 * 1024);
	}
	assert(count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
	set_nonblocking(fd, 0);
}

static void test_cancellation(const sigset_t *expected)
{
	for (int repeat = 0; repeat < 2; repeat++)
	{
		int fds[2];
		char byte = 'x';
		struct pollfd waitfd;

		assert(pipe(fds) == 0);
		reset_for_test();
		arm_timer(SIGTERM);
		assert(fvwmRead(fds[0], &byte, 1) == -1);
		assert(errno == EINTR && isTerminated && byte == 'x');
		assert_mask(expected);
		assert_disarmed(expected);

		fill_pipe(fds[1]);
		reset_for_test();
		arm_timer(SIGTERM);
		assert(fvwmWrite(fds[1], &byte, 1) == -1);
		assert(errno == EINTR && isTerminated);
		assert_mask(expected);
		assert_disarmed(expected);
		assert(close(fds[0]) == 0 && close(fds[1]) == 0);

		assert(pipe(fds) == 0);
		waitfd = (struct pollfd){fds[0], POLLIN, 0};
		reset_for_test();
		arm_timer(SIGTERM);
		assert(fvwmPoll(&waitfd, 1, -1) == -1);
		assert(errno == EINTR && isTerminated);
		assert_mask(expected);
		assert_disarmed(expected);
		assert(close(fds[0]) == 0 && close(fds[1]) == 0);
	}
}

static void test_preterminated(const sigset_t *expected)
{
	int fds[2];
	char byte = 'x';
	struct pollfd waitfd;

	assert(pipe(fds) == 0);
	assert(write(fds[1], "r", 1) == 1);
	assert(isTerminated);
	assert(fvwmRead(fds[0], &byte, 1) == -1);
	assert(errno == EINTR && byte == 'x');
	assert(read(fds[0], &byte, 1) == 1 && byte == 'r');
	assert(fvwmWrite(fds[1], "w", 1) == -1 && errno == EINTR);
	set_nonblocking(fds[0], 1);
	assert(read(fds[0], &byte, 1) == -1);
	assert(errno == EAGAIN || errno == EWOULDBLOCK);
	waitfd = (struct pollfd){fds[0], POLLIN, POLLOUT};
	assert(fvwmPoll(&waitfd, 1, -1) == -1 && errno == EINTR);
	assert(waitfd.revents == POLLOUT && isTerminated);
	assert_mask(expected);
	assert_disarmed(expected);
	assert(close(fds[0]) == 0 && close(fds[1]) == 0);
}

static void test_results(const sigset_t *expected)
{
	int fds[2];
	char buffer[16] = {0};
	struct pollfd waitfd;

	reset_for_test();
	assert(pipe(fds) == 0);
	assert(fvwmWrite(fds[1], "abc", 3) == 3);
	waitfd = (struct pollfd){fds[0], POLLIN, 0};
	assert(fvwmPoll(&waitfd, 1, 0) == 1 && (waitfd.revents & POLLIN));
	assert(fvwmRead(fds[0], buffer, sizeof(buffer)) == 3);
	assert(memcmp(buffer, "abc", 3) == 0);
	assert(fvwmPoll(&waitfd, 1, 10) == 0 && waitfd.revents == 0);
	assert(fvwmRead(-1, buffer, sizeof(buffer)) == -1 && errno == EBADF);
	assert(fvwmWrite(-1, buffer, sizeof(buffer)) == -1 && errno == EBADF);
	assert(close(fds[1]) == 0);
	assert(fvwmRead(fds[0], buffer, sizeof(buffer)) == 0);
	assert(close(fds[0]) == 0);
	assert(fvwmPoll(&waitfd, 1, 0) == 1);
	assert(waitfd.revents & POLLNVAL);
	assert(!isTerminated);
	assert_mask(expected);
	assert_disarmed(expected);
}

static void test_unrelated_interruptions(const sigset_t *expected)
{
	int fds[2];
	char byte = 'x';
	struct pollfd waitfd;

	reset_for_test();
	assert(pipe(fds) == 0);
	arm_timer(SIGUSR1);
	assert(fvwmRead(fds[0], &byte, 1) == -1 && errno == EINTR);
	assert(!isTerminated && unrelated_signals == 1 && byte == 'x');
	fill_pipe(fds[1]);
	arm_timer(SIGUSR1);
	assert(fvwmWrite(fds[1], &byte, 1) == -1 && errno == EINTR);
	assert(!isTerminated && unrelated_signals == 2);
	assert(close(fds[0]) == 0 && close(fds[1]) == 0);
	assert(pipe(fds) == 0);
	waitfd = (struct pollfd){fds[0], POLLIN, 0};
	arm_timer(SIGUSR1);
	assert(fvwmPoll(&waitfd, 1, -1) == -1 && errno == EINTR);
	assert(!isTerminated && unrelated_signals == 3);
	assert_mask(expected);
	assert_disarmed(expected);
	assert(close(fds[0]) == 0 && close(fds[1]) == 0);
}

int main(void)
{
	struct sigaction action;
	sigset_t original, expected;

	memset(&action, 0, sizeof(action));
	assert(sigemptyset(&action.sa_mask) == 0);
	action.sa_handler = terminate_handler;
	assert(sigaction(SIGTERM, &action, NULL) == 0);
	action.sa_handler = unrelated_handler;
	assert(sigaction(SIGUSR1, &action, NULL) == 0);
	action.sa_handler = timer_handler;
	assert(sigaction(SIGALRM, &action, NULL) == 0);
	assert(sigprocmask(SIG_SETMASK, NULL, &original) == 0);
	expected = original;
	assert(sigaddset(&expected, SIGUSR2) == 0);
	assert(sigdelset(&expected, SIGTERM) == 0);
	assert(sigdelset(&expected, SIGALRM) == 0);
	assert(sigdelset(&expected, SIGUSR1) == 0);
	assert(sigprocmask(SIG_SETMASK, &expected, NULL) == 0);
	assert(sigprocmask(SIG_SETMASK, NULL, &expected) == 0);
	test_cancellation(&expected);
	test_preterminated(&expected);
	test_results(&expected);
	test_unrelated_interruptions(&expected);
	assert(sigprocmask(SIG_SETMASK, &original, NULL) == 0);
	puts("actual signals preserve wrapper masks, lifetimes and I/O results");
	return 0;
}
