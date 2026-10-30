/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "modules/FvwmNotify/notify_startup.h"
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct
{
	pid_t pid;
	int ready, release;
} owned_child;

typedef enum
{
	BLOCKED_STARTUP,
	INHERITED_SIGNAL_STATE,
	CANCELED_STARTUP,
	INVALID_INTERVAL
} child_mode;

static double seconds_now(void)
{
	struct timespec value;

	if (clock_gettime(CLOCK_MONOTONIC, &value))
		return -1;
	return value.tv_sec + value.tv_nsec / 1000000000.0;
}

/* Retire the handle at actual Wait before any later cleanup signal. No PID
 * lookup or process-group signal substitutes for this child ownership. */
static int wait_until(owned_child *child, double deadline, int *status)
{
	for (;;)
	{
		pid_t result = waitpid(child->pid, status, WNOHANG);
		if (result == child->pid)
		{
			child->pid = -1;
			return 1;
		}
		if (result < 0 && errno != EINTR)
		{
			if (errno == ECHILD)
				child->pid = -1;
			return -1;
		}
		double now = seconds_now();
		if (now < 0)
			return -1;
		if (now >= deadline)
			return 0;
		(void)poll(NULL, 0, 10);
	}
}

static void retire(owned_child *child)
{
	int status;

	if (child->pid > 0)
	{
		(void)kill(child->pid, SIGKILL);
		/* These fixtures only block in an ordinary pipe read. Keep a
		 * fixed observation bound even when a broken guard did not fire. */
		for (unsigned int i = 0; i < 200 && child->pid > 0; i++)
		{
			pid_t result = waitpid(child->pid, &status, WNOHANG);
			if (result == child->pid ||
			    (result < 0 && errno == ECHILD))
				child->pid = -1;
			else
				(void)poll(NULL, 0, 10);
		}
	}
	close(child->ready);
	close(child->release);
}

static void require_child(owned_child *child, bool condition)
{
	if (!condition)
	{
		retire(child);
		assert(condition);
		abort();
	}
}

static bool signal_state(bool ignored, bool blocked)
{
	struct sigaction action = {0};
	sigset_t set;

	action.sa_handler = ignored ? SIG_IGN : SIG_DFL;
	return !sigemptyset(&action.sa_mask) &&
	    !sigaction(SIGALRM, &action, NULL) &&
	    !sigemptyset(&set) && !sigaddset(&set, SIGALRM) &&
	    !sigprocmask(blocked ? SIG_BLOCK : SIG_UNBLOCK, &set, NULL);
}

static bool invalid_intervals(void)
{
	struct sigaction action;
	sigset_t set;
	unsigned int remaining;

	if (!signal_state(true, true))
		return false;
	(void)alarm(5);
	if (notify_startup_begin(0) ||
	    notify_startup_begin(NOTIFY_STARTUP_SECONDS + 1) ||
	    sigaction(SIGALRM, NULL, &action) ||
	    sigprocmask(SIG_BLOCK, NULL, &set))
		return false;
	remaining = alarm(0);
	return action.sa_handler == SIG_IGN && sigismember(&set, SIGALRM) == 1 &&
	    remaining > 0 && remaining <= 5;
}

static owned_child start_child(child_mode mode)
{
	int ready[2], release[2];
	owned_child child;

	assert(pipe(ready) == 0);
	assert(pipe(release) == 0);
	child.pid = fork();
	assert(child.pid >= 0);
	if (!child.pid)
	{
		char byte;
		ssize_t result;

		close(ready[0]);
		close(release[1]);
		if (mode == INVALID_INTERVAL)
		{
			if (!invalid_intervals())
				_exit(91);
		}
		else
		{
			bool inherited = mode == INHERITED_SIGNAL_STATE;
			if (!signal_state(inherited, inherited) ||
			    !notify_startup_begin(1))
				_exit(92);
			if (mode == CANCELED_STARTUP)
				notify_startup_finish();
		}
		if (write(ready[1], "R", 1) != 1)
			_exit(93);
		close(ready[1]);
		if (mode == INVALID_INTERVAL)
			_exit(0);
		do
			result = read(release[0], &byte, 1);
		while (result < 0 && errno == EINTR);
		close(release[0]);
		notify_startup_finish();
		_exit(result == 1 && byte == 'Q' ? 0 : 94);
	}
	close(ready[1]);
	close(release[0]);
	child.ready = ready[0];
	child.release = release[1];
	struct pollfd descriptor = {child.ready, POLLIN, 0};
	char byte = 0;

	require_child(&child, poll(&descriptor, 1, 2000) > 0);
	require_child(&child, read(child.ready, &byte, 1) == 1 && byte == 'R');
	return child;
}

static void default_termination(child_mode mode)
{
	double started = seconds_now();
	int status = 0;
	owned_child child;

	assert(started >= 0);
	child = start_child(mode);
	require_child(&child, wait_until(&child, started + 4, &status) == 1);
	require_child(&child, WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM);
	require_child(&child, seconds_now() - started >= 0.75);
	retire(&child);
}

static void canceled_termination(void)
{
	owned_child child = start_child(CANCELED_STARTUP);
	double now = seconds_now();
	int status = 0;

	require_child(&child, now >= 0);
	/* Stay blocked past the one-second test interval after cancellation. */
	require_child(&child, wait_until(&child, now + 1.25, &status) == 0);
	require_child(&child, write(child.release, "Q", 1) == 1);
	now = seconds_now();
	require_child(&child, now >= 0);
	require_child(&child, wait_until(&child, now + 2, &status) == 1);
	require_child(&child, WIFEXITED(status) && WEXITSTATUS(status) == 0);
	retire(&child);
}

int main(void)
{
	struct sigaction action = {0};
	owned_child child;
	int status = 0;
	double now;

	/* The standalone unit is the sole owner of its direct child Waits. */
	action.sa_handler = SIG_DFL;
	assert(sigemptyset(&action.sa_mask) == 0);
	assert(sigaction(SIGCHLD, &action, NULL) == 0);
	action.sa_handler = SIG_IGN;
	assert(sigaction(SIGPIPE, &action, NULL) == 0);
	child = start_child(INVALID_INTERVAL);
	now = seconds_now();
	require_child(&child, now >= 0);
	require_child(&child, wait_until(&child, now + 2, &status) == 1);
	require_child(&child, WIFEXITED(status) && WEXITSTATUS(status) == 0);
	retire(&child);
	default_termination(BLOCKED_STARTUP);
	default_termination(INHERITED_SIGNAL_STATE);
	canceled_termination();
	return 0;
}
