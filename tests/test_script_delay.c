/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/script_delay.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t interrupted;

static void alarm_handler(int signal_number)
{
	(void)signal_number;
	interrupted = 1;
}

static int64_t nanoseconds(struct timespec time)
{
	return (int64_t)time.tv_sec * 1000000000 + time.tv_nsec;
}

static int64_t cpu_microseconds(struct rusage usage)
{
	return ((int64_t)usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) *
		1000000 + usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;
}

static void test_duration(void)
{
	struct timespec duration;

	script_delay_duration(INT_MIN, &duration);
	assert(duration.tv_sec == 0 && duration.tv_nsec == 0);
	script_delay_duration(0, &duration);
	assert(duration.tv_sec == 0 && duration.tv_nsec == 0);
	script_delay_duration(8, &duration);
	assert(duration.tv_sec == 0 && duration.tv_nsec == 133336000);
	script_delay_duration(16, &duration);
	assert(duration.tv_sec == 0 && duration.tv_nsec == 266672000);
	script_delay_duration(INT_MAX, &duration);
	assert(nanoseconds(duration) == (int64_t)INT_MAX * 16667000);
	assert(script_delay_ticks(0));
	assert(script_delay_ticks(INT_MIN));
}

static void test_periodic_timeout(void)
{
	struct timespec last = { 10, 900000000 };
	struct timespec now = last;

	assert(script_periodic_timeout_ms(&last, &now) == 1000);
	now.tv_nsec++;
	assert(script_periodic_timeout_ms(&last, &now) == 1000);
	now = (struct timespec){ 11, 899999999 };
	assert(script_periodic_timeout_ms(&last, &now) == 1);
	now.tv_nsec++;
	assert(script_periodic_timeout_ms(&last, &now) == 0);
	now = (struct timespec){ 12, 0 };
	assert(script_periodic_timeout_ms(&last, &now) == 0);
	now = (struct timespec){ 10, 899999999 };
	assert(script_periodic_timeout_ms(&last, &now) == 1000);
	now = (struct timespec){ 0, 0 };
	assert(script_periodic_timeout_ms(&last, &now) == 1000);
	last = (struct timespec){ 0, 0 };
	now = (struct timespec){ INT_MAX, 999999999 };
	assert(script_periodic_timeout_ms(&last, &now) == 0);
}

static void test_readiness_timeout(void)
{
	struct timespec last = { 10, 0 };
	struct timespec now = last;

	assert(script_readiness_timeout_ms(false, false, NULL, NULL) == -1);
	assert(script_readiness_timeout_ms(false, true, NULL, NULL) == 10);
	assert(script_readiness_timeout_ms(true, false, &last, &now) == 1000);
	assert(script_readiness_timeout_ms(true, true, &last, &now) == 10);
	now.tv_nsec = 999000000;
	assert(script_readiness_timeout_ms(true, false, &last, &now) == 1);
	assert(script_readiness_timeout_ms(true, true, &last, &now) == 1);
	now.tv_sec++;
	assert(script_readiness_timeout_ms(true, true, &last, &now) == 0);
}

static void test_interrupted_sleep_does_not_spin(void)
{
	struct sigaction action = {0}, previous;
	struct itimerval timer = {0}, old_timer;
	struct timespec start, end;
	struct rusage before, after;

	action.sa_handler = alarm_handler;
	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGALRM, &action, &previous) == 0);
	timer.it_value.tv_usec = 10000;
	assert(getrusage(RUSAGE_SELF, &before) == 0);
	assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
	assert(setitimer(ITIMER_REAL, &timer, &old_timer) == 0);
	assert(script_delay_ticks(16));
	assert(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
	assert(getrusage(RUSAGE_SELF, &after) == 0);
	assert(interrupted);
	assert(nanoseconds(end) - nanoseconds(start) >= 266672000);
	/* Leave ample room for emulation and signal delivery, while detecting
	 * the old full-core busy wait over this quarter-second interval. */
	assert(cpu_microseconds(after) - cpu_microseconds(before) < 100000);
	assert(setitimer(ITIMER_REAL, &old_timer, NULL) == 0);
	assert(sigaction(SIGALRM, &previous, NULL) == 0);
}

typedef struct
{
	int fd;
	int ordinary;
	bool cancelled;
	bool watch_signal;
} input_state;

static bool cancel_input(void *context)
{
	input_state *state = context;
	char byte;
	ssize_t count;

	if (state->cancelled || (state->watch_signal && interrupted))
	{
		return true;
	}
	count = read(state->fd, &byte, 1);
	if (count == 1)
	{
		if (byte == 'R')
		{
			state->cancelled = true;
			return true;
		}
		state->ordinary++;
	}
	else if (count < 0)
	{
		assert(errno == EAGAIN || errno == EINTR);
	}
	return false;
}

static bool never_cancel(void *context)
{
	(void)context;
	return false;
}

static void input_pipe(int pipes[2])
{
	int high_fd;

	assert(pipe(pipes) == 0);
	high_fd = fcntl(pipes[0], F_DUPFD, 80);
	assert(high_fd >= 80);
	assert(close(pipes[0]) == 0);
	pipes[0] = high_fd;
	assert(fcntl(pipes[0], F_SETFL, O_NONBLOCK) == 0);
}

static void test_queued_and_arriving_cancellation(void)
{
	int pipes[2], status;
	struct timespec start, end;
	pid_t child;
	input_state state = {0};

	input_pipe(pipes);
	state.fd = pipes[0];
	assert(write(pipes[1], "R", 1) == 1);
	assert(script_delay_wait(INT_MAX, pipes[0], cancel_input, &state) ==
		SCRIPT_DELAY_CANCELLED);
	state.cancelled = false;
	child = fork();
	assert(child >= 0);
	if (child == 0)
	{
		close(pipes[0]);
		if (!script_delay_ticks(2) || write(pipes[1], "X", 1) != 1 ||
		    !script_delay_ticks(2) || write(pipes[1], "R", 1) != 1)
		{
			_exit(1);
		}
		_exit(0);
	}
	assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
	assert(script_delay_wait(60, pipes[0], cancel_input, &state) ==
		SCRIPT_DELAY_CANCELLED);
	assert(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
	assert(state.ordinary == 1 && state.cancelled);
	assert(nanoseconds(end) - nanoseconds(start) < 1000020000);
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	assert(close(pipes[0]) == 0);
	assert(close(pipes[1]) == 0);
}

static void test_other_input_keeps_deadline(void)
{
	int pipes[2], status;
	struct timespec start, end;
	pid_t child;
	input_state state = {0};

	input_pipe(pipes);
	state.fd = pipes[0];
	child = fork();
	assert(child >= 0);
	if (child == 0)
	{
		close(pipes[0]);
		for (int i = 0; i < 32; i++)
		{
			if (!script_delay_ticks(1) ||
			    write(pipes[1], "X", 1) != 1)
			{
				_exit(1);
			}
		}
		_exit(0);
	}
	assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
	assert(script_delay_wait(60, pipes[0], cancel_input, &state) ==
		SCRIPT_DELAY_ELAPSED);
	assert(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
	assert(state.ordinary > 0);
	assert(nanoseconds(end) - nanoseconds(start) >= 1000020000);
	assert(nanoseconds(end) - nanoseconds(start) < 1450000000);
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	assert(close(pipes[0]) == 0);
	assert(close(pipes[1]) == 0);
}

static void test_signal_cancellation_and_descriptor_errors(void)
{
	int pipes[2];
	struct sigaction action = {0}, previous;
	struct itimerval timer = {0}, old_timer;
	input_state state = {.watch_signal = true};

	input_pipe(pipes);
	state.fd = pipes[0];
	interrupted = 0;
	action.sa_handler = alarm_handler;
	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGALRM, &action, &previous) == 0);
	timer.it_value.tv_usec = 10000;
	assert(setitimer(ITIMER_REAL, &timer, &old_timer) == 0);
	assert(script_delay_wait(INT_MAX, pipes[0], cancel_input, &state) ==
		SCRIPT_DELAY_CANCELLED);
	assert(interrupted);
	assert(setitimer(ITIMER_REAL, &old_timer, NULL) == 0);
	assert(sigaction(SIGALRM, &previous, NULL) == 0);
	assert(script_delay_wait(1, -2, NULL, NULL) == SCRIPT_DELAY_ERROR);
	assert(errno == EINVAL);
	assert(script_delay_wait(1, pipes[0], NULL, NULL) ==
		SCRIPT_DELAY_ERROR);
	assert(errno == EINVAL);
	assert(close(pipes[1]) == 0);
	assert(script_delay_wait(1, pipes[0], never_cancel, NULL) ==
		SCRIPT_DELAY_ERROR);
	assert(errno == EIO);
	assert(close(pipes[0]) == 0);
	assert(script_delay_wait(1, pipes[0], never_cancel, NULL) ==
		SCRIPT_DELAY_ERROR);
	assert(errno == EBADF);
}

int main(void)
{
	test_duration();
	test_periodic_timeout();
	test_readiness_timeout();
	test_interrupted_sleep_does_not_spin();
	test_queued_and_arriving_cancellation();
	test_other_input_keeps_deadline();
	test_signal_cancellation_and_descriptor_errors();
	return 0;
}
