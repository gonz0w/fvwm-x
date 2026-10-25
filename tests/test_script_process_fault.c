/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/script_process.h"
#include "libs/fvwmsignal.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

/* Meson redirects glibc large-file calls; BSD/musl use the POSIX symbol. */
#if defined(__GLIBC__) && defined(__USE_FILE_OFFSET64)
#define __wrap_fcntl __wrap_fcntl64
#define __real_fcntl __real_fcntl64
#endif

/* Linux linker wrapping keeps injection out of the production constructor. */
enum fault_kind {
	FAULT_NONE, FAULT_PIPE, FAULT_GETFD, FAULT_SETFD, FAULT_GETFL,
	FAULT_SETFL, FAULT_MASK_BLOCK, FAULT_MASK_RESTORE, FAULT_FORK,
	FAULT_GROUP, FAULT_DUP, FAULT_EXEC, FAULT_ACTION
};

static enum fault_kind fault;
static pid_t owner;
static bool fail_child, high_pipes, fail_fork_restore;
static int occurrence, calls;
static bool monitor_waits, in_shared_wait, cancel_before_wait;
static int shared_waits, foreground_polls, cleanup_polls;
static int canceled_foreground_polls, longest_open_wait, closed_waits;

int __real_fvwmPoll(struct pollfd *fds, nfds_t count, int timeout_ms);
int __real_poll(struct pollfd *fds, nfds_t count, int timeout_ms);
int __real_pipe(int descriptors[2]);
int __real_fcntl(int descriptor, int command, ...);
int __real_sigprocmask(int how, const sigset_t *set, sigset_t *old);
int __real_sigaction(int number, const struct sigaction *action,
	struct sigaction *old);
pid_t __real_fork(void);
int __real_setpgid(pid_t pid, pid_t group);
int __real_dup2(int old, int new);
int __real_execl(const char *path, const char *arg, ...);

int __wrap_fvwmPoll(struct pollfd *fds, nfds_t count, int timeout_ms)
{
	if (!monitor_waits || getpid() != owner)
		return __real_fvwmPoll(fds, count, timeout_ms);
	assert(fds != NULL && count == 2);
	shared_waits++;
	if (cancel_before_wait)
	{
		cancel_before_wait = false;
		/* Hit the foreground check/enter boundary, before the real shared
		 * helper arms its jump and checks the permanent termination flag. */
		assert(raise(SIGTERM) == 0);
	}
	in_shared_wait = true;
	int result = __real_fvwmPoll(fds, count, timeout_ms);
	in_shared_wait = false;
	return result;
}

int __wrap_poll(struct pollfd *fds, nfds_t count, int timeout_ms)
{
	if (monitor_waits && getpid() == owner)
	{
		if (fds == NULL)
		{
			/* TERM cleanup intentionally waits outside the shared helper. */
			assert(count == 0);
			assert(!in_shared_wait);
			cleanup_polls++;
		}
		else
		{
			assert(count == 2);
			foreground_polls++;
			if (isTerminated)
				canceled_foreground_polls++;
			if (fds[0].fd < 0 && fds[1].fd < 0)
			{
				assert(timeout_ms > 0 && timeout_ms <= 10);
				closed_waits++;
			}
			else if (timeout_ms > longest_open_wait)
				longest_open_wait = timeout_ms;
		}
	}
	return __real_poll(fds, count, timeout_ms);
}

static bool refuse(enum fault_kind kind)
{
	if (fault != kind || (getpid() != owner) != fail_child)
		return false;
	if (++calls != occurrence)
		return false;
	errno = EIO;
	return true;
}

int __wrap_pipe(int descriptors[2])
{
	if (refuse(FAULT_PIPE))
		return -1;
	int result = __real_pipe(descriptors);
	if (result == 0 && high_pipes)
	{
		for (int i = 0; i < 2; i++)
		{
			int replacement = __real_fcntl(descriptors[i],
				F_DUPFD_CLOEXEC, 4096);
			assert(replacement >= 4096);
			close(descriptors[i]);
			descriptors[i] = replacement;
		}
	}
	return result;
}

int __wrap_fcntl(int descriptor, int command, ...)
{
	enum fault_kind kind = FAULT_NONE;

	switch (command)
	{
	case F_GETFD: kind = FAULT_GETFD; break;
	case F_SETFD: kind = FAULT_SETFD; break;
	case F_GETFL: kind = FAULT_GETFL; break;
	case F_SETFL: kind = FAULT_SETFL; break;
	}
	if (kind != FAULT_NONE && refuse(kind))
		return -1;
	if (command == F_GETFD || command == F_GETFL)
		return __real_fcntl(descriptor, command);
	va_list arguments;
	va_start(arguments, command);
	int argument = va_arg(arguments, int);
	va_end(arguments);
	return __real_fcntl(descriptor, command, argument);
}

int __wrap_sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
	if (set != NULL &&
	    refuse(how == SIG_BLOCK ? FAULT_MASK_BLOCK : FAULT_MASK_RESTORE))
		return -1;
	/* Fork refusal and its first cleanup restoration fail independently. */
	if (fail_fork_restore && set != NULL && how == SIG_SETMASK &&
	    getpid() == owner)
	{
		fail_fork_restore = false;
		errno = EPERM;
		return -1;
	}
	return __real_sigprocmask(how, set, old);
}

int __wrap_sigaction(int number, const struct sigaction *action,
	struct sigaction *old)
{
	if (refuse(FAULT_ACTION))
		return -1;
	return __real_sigaction(number, action, old);
}

pid_t __wrap_fork(void)
{
	if (refuse(FAULT_FORK))
		return -1;
	return __real_fork();
}

int __wrap_setpgid(pid_t pid, pid_t group)
{
	if (refuse(FAULT_GROUP))
		return -1;
	return __real_setpgid(pid, group);
}

int __wrap_dup2(int old, int new)
{
	if (refuse(FAULT_DUP))
		return -1;
	return __real_dup2(old, new);
}

int __wrap_execl(const char *path, const char *arg, ...)
{
	if (refuse(FAULT_EXEC))
		return -1;
	va_list arguments;
	va_start(arguments, arg);
	const char *option = va_arg(arguments, const char *);
	const char *command = va_arg(arguments, const char *);
	assert(va_arg(arguments, const char *) == NULL);
	va_end(arguments);
	return __real_execl(path, arg, option, command, (char *)NULL);
}

static int descriptor_count(void)
{
	DIR *directory = opendir("/proc/self/fd");
	struct dirent *entry;
	int count = 0;

	assert(directory != NULL);
	while ((entry = readdir(directory)) != NULL)
	{
		if (entry->d_name[0] != '.')
			count++;
	}
	assert(closedir(directory) == 0);
	return count;
}

static void assert_mask(const sigset_t *expected)
{
	sigset_t actual;

	assert(sigprocmask(SIG_SETMASK, NULL, &actual) == 0);
	for (int number = 1; number < NSIG; number++)
		assert(sigismember(&actual, number) ==
			sigismember(expected, number));
}

static void retry(void)
{
	char *text;
	size_t length;

	assert(script_process_read("printf retry; exit 127", 1000,
		&text, &length));
	assert(length == 5 && strcmp(text, "retry") == 0);
	free(text);
}

static void test_fault(enum fault_kind kind, bool child, int nth)
{
	char *text = (char *)1;
	size_t length = 99;
	sigset_t previous;
	int descriptors = descriptor_count();

	assert(sigprocmask(SIG_SETMASK, NULL, &previous) == 0);
	owner = getpid();
	calls = 0;
	occurrence = nth;
	fail_child = child;
	fault = kind;
	bool result = script_process_read("printf forbidden", 1000,
		&text, &length);
	if (result || errno != EIO || text != NULL || length != 0)
		fprintf(stderr, "fault kind=%d child=%d nth=%d calls=%d "
			"result=%d errno=%d length=%zu\n", kind, child, nth,
			calls, result, errno, length);
	assert(!result);
	assert(errno == EIO && text == NULL && length == 0);
	fault = FAULT_NONE;
	assert(descriptor_count() == descriptors);
	assert_mask(&previous);
	assert(waitpid(-1, NULL, WNOHANG) < 0 && errno == ECHILD);
	retry();
	assert(descriptor_count() == descriptors);
}

static void test_stdout_alias(bool close_stdin)
{
	int saved_stdout = fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 10);
	int saved_stdin = -1;

	assert(saved_stdout >= 10);
	if (close_stdin)
	{
		saved_stdin = fcntl(STDIN_FILENO, F_DUPFD_CLOEXEC, 10);
		assert(saved_stdin >= 10);
		assert(close(STDIN_FILENO) == 0);
	}
	assert(close(STDOUT_FILENO) == 0);
	retry();
	assert(dup2(saved_stdout, STDOUT_FILENO) == STDOUT_FILENO);
	close(saved_stdout);
	if (close_stdin)
	{
		assert(dup2(saved_stdin, STDIN_FILENO) == STDIN_FILENO);
		close(saved_stdin);
	}
}

static void start_wait_monitor(void)
{
	owner = getpid();
	shared_waits = foreground_polls = cleanup_polls = 0;
	canceled_foreground_polls = longest_open_wait = closed_waits = 0;
	monitor_waits = true;
}

static void test_foreground_routing(void)
{
	char *text;
	size_t length;

	start_wait_monitor();
	assert(script_process_read("sleep 0.05; printf routed", 1000,
		&text, &length));
	assert(length == 6 && strcmp(text, "routed") == 0);
	free(text);
	monitor_waits = false;
	if (shared_waits == 0 || longest_open_wait <= 100)
		fprintf(stderr, "foreground shared waits=%d raw polls=%d "
			"longest open timeout=%d ms\n", shared_waits,
			foreground_polls, longest_open_wait);
	assert(shared_waits > 0 && shared_waits == foreground_polls);
	assert(longest_open_wait > 100 && longest_open_wait <= 1000);
	assert(cleanup_polls == 0);

	start_wait_monitor();
	assert(script_process_read("exec 1>&-; sleep 0.1", 1000,
		&text, &length));
	assert(length == 0 && strcmp(text, "") == 0);
	free(text);
	monitor_waits = false;
	assert(closed_waits > 0);
	assert(shared_waits == foreground_polls && cleanup_polls == 0);
}

static void test_foreground_entry_cancellation(void)
{
	struct sigaction action = {0}, previous;
	char *text = (char *)1;
	size_t length = 99;
	int descriptors = descriptor_count();

	action.sa_handler = fvwmSetTerminate;
	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGTERM, &action, &previous) == 0);
	start_wait_monitor();
	cancel_before_wait = true;
	assert(!script_process_read("sleep 2", 1000, &text, &length));
	assert(errno == ECANCELED && text == NULL && length == 0);
	monitor_waits = false;
	assert(shared_waits == 1 && !cancel_before_wait && isTerminated);
	assert(foreground_polls == 0 && canceled_foreground_polls == 0);
	assert(cleanup_polls > 0);
	assert(descriptor_count() == descriptors);
	assert(waitpid(-1, NULL, WNOHANG) < 0 && errno == ECHILD);
	/* Production termination is permanent; only this fixture resets it. */
	isTerminated = 0;
	assert(sigaction(SIGTERM, &previous, NULL) == 0);
	retry();
}

int main(void)
{
	test_foreground_routing();
	test_foreground_entry_cancellation();
	for (int nth = 1; nth <= 2; nth++)
		test_fault(FAULT_PIPE, false, nth);
	for (int nth = 1; nth <= 4; nth++)
	{
		test_fault(FAULT_GETFD, false, nth);
		test_fault(FAULT_SETFD, false, nth);
		test_fault(FAULT_ACTION, true, nth);
	}
	for (enum fault_kind kind = FAULT_GETFL; kind <= FAULT_ACTION; kind++)
	{
		if (kind == FAULT_EXEC || kind == FAULT_DUP)
			test_fault(kind, true, 1);
		else
			test_fault(kind, false, 1);
	}
	fail_fork_restore = true;
	test_fault(FAULT_FORK, false, 1);
	test_fault(FAULT_GROUP, true, 1);
	test_fault(FAULT_MASK_RESTORE, true, 1);
	test_fault(FAULT_GETFD, true, 1);
	test_fault(FAULT_SETFD, true, 1);
	test_stdout_alias(false);
	test_stdout_alias(true);
	struct rlimit limit;
	assert(getrlimit(RLIMIT_NOFILE, &limit) == 0);
	if (limit.rlim_cur <= 4100 && limit.rlim_max > 4100)
	{
		struct rlimit raised = limit;
		raised.rlim_cur = 4101;
		assert(setrlimit(RLIMIT_NOFILE, &raised) == 0);
	}
	struct rlimit current;
	assert(getrlimit(RLIMIT_NOFILE, &current) == 0);
	if (current.rlim_cur > 4100)
	{
		high_pipes = true;
		retry();
		high_pipes = false;
	}
	else
		puts("high descriptors unavailable under inherited hard limit");
	assert(setrlimit(RLIMIT_NOFILE, &limit) == 0);
	return 0;
}
