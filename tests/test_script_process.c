/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/script_process.h"
#include "modules/FvwmScript/script_output.h"
#include "libs/fvwmsignal.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t alarms;

static void alarm_received(int signal_number)
{
	(void)signal_number;
	alarms++;
}

static double monotonic_seconds(void)
{
	struct timespec now;
	assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
	return now.tv_sec + now.tv_nsec / 1000000000.0;
}

static void test_valid_output(void)
{
	char *text;
	size_t length;

	assert(script_process_read("printf 'first\\nsecond'; exit 3", 1000,
		&text, &length));
	assert(length == 12 && strcmp(text, "first\nsecond") == 0);
	free(text);
	assert(script_process_read("printf exit127; exit 127", 1000,
		&text, &length));
	assert(strcmp(text, "exit127") == 0);
	free(text);
	assert(script_process_read("printf ''", 1000, &text, &length));
	assert(length == 0 && strcmp(text, "") == 0);
	free(text);
	assert(!script_process_read("printf '\\000'", 1000, &text, &length));
	assert(text == NULL && length == 0 && errno == EINVAL);
}

static void test_output_prefix(void)
{
	char path[] = "/tmp/fvwm-process-output-XXXXXX";
	char command[256], *text;
	size_t length;
	int descriptor = mkstemp(path);

	assert(descriptor >= 0);
	char bytes[1000];
	memset(bytes, 'x', sizeof(bytes));
	for (int i = 0; i < 64; i++)
		assert(write(descriptor, bytes, sizeof(bytes)) ==
			sizeof(bytes));
	assert(close(descriptor) == 0);
	snprintf(command, sizeof(command), "cat %s", path);
	assert(script_process_read(command, 1000, &text, &length));
	assert(length == SCRIPT_OUTPUT_MAX_BYTES && text[length] == '\0');
	assert(strspn(text, "x") == length);
	free(text);
	assert(unlink(path) == 0);
}

static void test_timeout(const char *command)
{
	char *text = (char *)1;
	size_t length = 99;
	double started = monotonic_seconds();

	assert(!script_process_read(command, 50, &text, &length));
	assert(errno == ETIMEDOUT && text == NULL && length == 0);
	assert(monotonic_seconds() - started < 3);
	/* Cleanup reaps the direct child, including a shell that already exited
	 * while a background writer retained its output pipe. */
	assert(waitpid(-1, NULL, WNOHANG) == -1 && errno == ECHILD);
}

static void test_group_cleanup(void)
{
	char path[] = "/tmp/fvwm-process-group-XXXXXX";
	char command[512], *text;
	size_t length;
	int descriptor = mkstemp(path);

	assert(descriptor >= 0);
	close(descriptor);
	assert(unlink(path) == 0);
	snprintf(command, sizeof(command),
		"(sleep 1; printf late > %s) & wait", path);
	assert(!script_process_read(command, 50, &text, &length));
	assert(errno == ETIMEDOUT);
	assert(script_process_read("sleep 1.2; printf recovery", 3000,
		&text, &length));
	assert(strcmp(text, "recovery") == 0);
	free(text);
	assert(access(path, F_OK) == -1 && errno == ENOENT);
}

static void test_interrupted_waits(void)
{
	struct sigaction action = {0}, previous;
	struct itimerval timer = {{0, 10000}, {0, 10000}};
	char *text;
	size_t length;

	action.sa_handler = alarm_received;
	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGALRM, &action, &previous) == 0);
	assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
	assert(script_process_read("sleep 0.1; printf done", 1000,
		&text, &length));
	assert(strcmp(text, "done") == 0 && alarms > 0);
	free(text);
	test_timeout("sleep 2");
	memset(&timer, 0, sizeof(timer));
	assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
	assert(sigaction(SIGALRM, &previous, NULL) == 0);
}

static void test_invalid_policy(void)
{
	char *text;
	size_t length;
	struct sigaction action = {0}, previous;

	assert(!script_process_read("printf x", 0, &text, &length));
	assert(!script_process_read("printf x", -1, &text, &length));
	assert(!script_process_read("printf x",
		SCRIPT_PROCESS_MAX_TIMEOUT_MS + 1, &text, &length));
	action.sa_handler = SIG_IGN;
	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGCHLD, &action, &previous) == 0);
	assert(!script_process_read("printf x", 1000, &text, &length));
	assert(text == NULL && length == 0 && errno == EINVAL);
	assert(sigaction(SIGCHLD, &previous, NULL) == 0);
}

static void test_cancellation(void)
{
	struct sigaction action = {0}, previous;
	struct itimerval timer = {{0, 0}, {0, 50000}};
	char *text = (char *)1;
	size_t length = 99;
	char path[] = "/tmp/fvwm-process-cancel-XXXXXX";
	char command[512];
	int descriptor = mkstemp(path);

	assert(descriptor >= 0);
	close(descriptor);
	assert(unlink(path) == 0);
	isTerminated = 1;
	snprintf(command, sizeof(command), "printf spawned > %s", path);
	assert(!script_process_read(command, 3000, &text, &length));
	assert(errno == ECANCELED && text == NULL && length == 0);
	assert(access(path, F_OK) < 0 && errno == ENOENT);
	isTerminated = 0;
	action.sa_handler = fvwmSetTerminate;
	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGALRM, &action, &previous) == 0);
	for (int closed = 0; closed < 2; closed++)
	{
		snprintf(command, sizeof(command),
			"%s(sleep 1; printf late > %s) & wait",
			closed ? "exec 1>&-; " : "printf partial; ", path);
		assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
		double started = monotonic_seconds();
		assert(!script_process_read(command, 3000, &text, &length));
		assert(errno == ECANCELED && text == NULL && length == 0);
		assert(monotonic_seconds() - started < 1);
		assert(waitpid(-1, NULL, WNOHANG) == -1 && errno == ECHILD);
		isTerminated = 0;
		assert(script_process_read("sleep 1.1; printf recovery", 3000,
			&text, &length));
		assert(strcmp(text, "recovery") == 0);
		free(text);
		assert(access(path, F_OK) < 0 && errno == ENOENT);
	}
	assert(sigaction(SIGALRM, &previous, NULL) == 0);
}

int main(void)
{
	test_invalid_policy();
	test_cancellation();
	test_valid_output();
	test_output_prefix();
	test_timeout("printf partial; sleep 2");
	test_timeout("exec 1>&-; sleep 2");
	test_timeout("(sleep 2; printf late) &");
	test_timeout("trap '' TERM; sleep 2");
	test_group_cleanup();
	test_interrupted_waits();
	return 0;
}
