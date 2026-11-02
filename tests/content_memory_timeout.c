/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Private qualification helper for frozen non-forking probes only. Keep the
 * direct child unreaped while signaling: its PID cannot name another
 * process. A failed/unconfirmed wait aborts the job; its owner then stops
 * the whole VM. */
static volatile sig_atomic_t cancel_signal;

static void interrupted(int signal)
{
	if (!cancel_signal)
		cancel_signal = signal;
}

static uint64_t now_ns(void)
{
	struct timespec value;

	if (clock_gettime(CLOCK_MONOTONIC, &value) < 0 || value.tv_sec < 0 ||
		value.tv_nsec < 0 || value.tv_nsec >= 1000000000L ||
		(uint64_t)value.tv_sec >
		(UINT64_MAX - (uint64_t)value.tv_nsec) / 1000000000ULL)
		return 0;
	return (uint64_t)value.tv_sec * 1000000000ULL + value.tv_nsec;
}

int main(int argc, char **argv)
{
	struct sigaction action = {0}, original = {0};
	const int signals[] = {SIGINT, SIGTERM, SIGHUP};
	const struct timespec pause = {0, 1000000};
	char *end;
	unsigned long seconds;
	int status = 0, code = 125;
	pid_t child, waited = 0;
	FILE *metrics;
	uint64_t start, finish, deadline, grace = 0;
	bool failed = false, timed_out = false, terminated = false;
	bool killed = false;

	if (argc < 4 || argv[1][0] < '0' || argv[1][0] > '9')
		return 125;
	errno = 0;
	seconds = strtoul(argv[1], &end, 10);
	if (errno || *end || seconds < 1 || seconds > 30)
		return 125;
	metrics = fopen(argv[2], "wx");
	if (!metrics)
		return 125;
	if (fcntl(fileno(metrics), F_SETFD, FD_CLOEXEC) < 0)
	{
		fclose(metrics);
		return 125;
	}
	sigemptyset(&action.sa_mask);
	for (size_t i = 0; i < sizeof(signals) / sizeof(*signals); i++)
		sigaddset(&action.sa_mask, signals[i]);
	action.sa_handler = interrupted;
	for (size_t i = 0; i < sizeof(signals) / sizeof(*signals); i++)
		if (sigaction(signals[i], &action, NULL) < 0)
		{
			fclose(metrics);
			return 125;
		}
	/* Deadline polling needs no alarm signal. Explicitly unblock owned
	 * cancellation signals so inherited masks cannot disable delivery. */
	if (sigprocmask(SIG_UNBLOCK, &action.sa_mask, NULL) < 0)
	{
		fclose(metrics);
		return 125;
	}
	sigemptyset(&original.sa_mask);
	original.sa_handler = SIG_DFL;
	start = now_ns();
	if (sigaction(SIGCHLD, &original, NULL) < 0 || !start ||
		start > UINT64_MAX - 40000000000ULL)
	{
		fclose(metrics);
		return 125;
	}
	deadline = start + seconds * 1000000000ULL;
	child = fork();
	if (child < 0)
	{
		fclose(metrics);
		return 125;
	}
	if (!child)
	{
		for (size_t i = 0; i < sizeof(signals) / sizeof(*signals); i++)
			if (sigaction(signals[i], &original, NULL) < 0)
				_exit(126);
		execv(argv[3], argv + 3);
		_exit(126);
	}
	for (;;)
	{
		uint64_t now;

		waited = waitpid(child, &status, WNOHANG);
		if (waited == child)
			break;
		if (waited < 0 && errno != EINTR)
		{
			failed = true;
			break;
		}
		now = now_ns();
		if (!now || now < start || now > UINT64_MAX - 6000000000ULL)
		{
			/* The child remains owned/unreaped. Refuse qualification and
			 * let job failure stop the disposable VM if wait is not ready. */
			kill(child, SIGKILL);
			waited = waitpid(child, &status, WNOHANG);
			failed = true;
			break;
		}
		if (!terminated && (cancel_signal || now >= deadline))
		{
			timed_out = now >= deadline;
			terminated = true;
			grace = now + 1000000000ULL;
			if (kill(child, SIGTERM) < 0 && errno != ESRCH)
				failed = true;
		}
		else if (terminated && !killed && now >= grace)
		{
			killed = true;
			if (kill(child, SIGKILL) < 0 && errno != ESRCH)
				failed = true;
		}
		if (killed && now >= grace && now - grace >= 5000000000ULL)
		{
			failed = true;
			break;
		}
		/* Polling has no flag-to-blocking-wait race. One millisecond is
		 * requested cadence; scheduler delay can exceed that interval. */
		nanosleep(&pause, NULL);
	}
	finish = now_ns();
	if (finish >= deadline)
		timed_out = true;
	if (waited == child)
		code = WIFEXITED(status) ? WEXITSTATUS(status) :
			WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 125;
	if (waited != child || !finish || finish < start)
		failed = true;
	if (fprintf(metrics, "{\"child_wait\":%d,\"wait_confirmed\":%s,"
		"\"child_signal\":%d,\"sigsegv\":%d,\"sigbus\":%d,"
		"\"timed_out\":%s,\"cancel_signal\":%d,"
		"\"supervised_wall_ns\":%" PRIu64 ",\"poll_ns\":1000000}\n",
		code, waited == child ? "true" : "false",
		waited == child && WIFSIGNALED(status) ? WTERMSIG(status) : 0,
		SIGSEGV, SIGBUS, timed_out ? "true" : "false", (int)cancel_signal,
		finish >= start ? finish - start : 0) < 0)
		failed = true;
	if (fclose(metrics) == EOF)
		failed = true;
	if (failed)
		return 125;
	return cancel_signal ? 128 + cancel_signal : timed_out ? 124 : code;
}
