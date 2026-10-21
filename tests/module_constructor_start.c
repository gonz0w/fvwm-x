/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/module_records.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/commands.h"
#include "fvwm/events.h"
#include "fvwm/module_list.h"
#include "fvwm/session.h"
#include "libs/FSMlib.h"

static FIceIOErrorHandler owned, candidate, observed_default;
static unsigned prior_calls, connection_attempts;
FIceIOErrorHandler __real_IceSetIOErrorHandler(FIceIOErrorHandler);

static void prior_handler(FIceConn connection)
{
	(void)connection;
	prior_calls++;
}

FIceIOErrorHandler __wrap_IceSetIOErrorHandler(FIceIOErrorHandler handler)
{
	FIceIOErrorHandler previous = __real_IceSetIOErrorHandler(handler);
	if (handler == NULL)
	{
		candidate = previous;
	}
	else
	{
		owned = handler;
		observed_default = previous;
	}
	return previous;
}

FSmcConn __wrap_SmcOpenConnection(char *network, FSmPointer context,
	int major, int minor, unsigned long mask, FSmcCallbacks *callbacks,
	const char *previous_id, char **client_id, int length, char *error)
{
	(void)network; (void)context; (void)major; (void)minor; (void)mask;
	(void)callbacks; (void)previous_id; (void)length;
	*client_id = NULL;
	error[0] = 0;
	connection_attempts++;
	return NULL;
}

static void check_ice_retry(void)
{
	if (!SessionSupport)
	{
		return;
	}
	FIceIOErrorHandler original = __real_IceSetIOErrorHandler(prior_handler);

	prior_calls = 0;
	for (unsigned attempt = 0; attempt < 3; attempt++)
	{
		unsigned before = connection_attempts;

		SessionInit();
		assert(connection_attempts == before + 1 && owned != NULL);
		assert(observed_default != owned);
		assert(candidate == (attempt == 0 ? prior_handler : owned));
		owned(NULL);
		assert(prior_calls == attempt + 1);
	}
	__real_IceSetIOErrorHandler(original);
	puts("core ICE retries preserve the prior callback exactly once");
}

static Bool refuse_fork;
static unsigned fork_calls;
static pid_t child_pid;
static Bool measure_arguments;
static unsigned argument_resizes;
static size_t argument_capacity;
static char **argument_vector;
static unsigned expected_count;
static char expected_tokens[256][32];

void *__real_fxrealloc(void *, size_t, size_t);
void *__wrap_fxrealloc(void *previous, size_t count, size_t size)
{
	void *result = __real_fxrealloc(previous, count, size);

	if (measure_arguments && size == sizeof(char *))
	{
		argument_resizes++;
		argument_capacity = count;
		argument_vector = result;
	}
	return result;
}

static void check_argument_vector(void)
{
	if (!measure_arguments || expected_count == 0)
	{
		return;
	}
	assert(argument_vector != NULL &&
		argument_capacity > 6 + expected_count);
	assert(strcmp(argument_vector[0], "/usr/bin/true") == 0);
	for (unsigned i = 1; i < 6; i++)
	{
		assert(argument_vector[i] != NULL);
	}
	for (unsigned i = 0; i < expected_count; i++)
	{
		assert(strcmp(argument_vector[6 + i], expected_tokens[i]) == 0);
	}
	assert(argument_vector[6 + expected_count] == NULL);
}

pid_t __real_fork(void);
pid_t __wrap_fork(void)
{
	fork_calls++;
	check_argument_vector();
	if (refuse_fork)
	{
		errno = EAGAIN;
		return -1;
	}
	pid_t child = __real_fork();
	if (child > 0)
	{
		child_pid = child;
	}
	return child;
}

static unsigned snapshot(unsigned char *open_fds)
{
	unsigned count = 0;

	for (unsigned fd = 0; fd < FD_SETSIZE; fd++)
	{
		int result = fcntl(fd, F_GETFD);
		assert(result >= 0 || errno == EBADF);
		open_fds[fd] = result >= 0;
		count += open_fds[fd];
	}
	return count;
}

static unsigned module_count(void)
{
	fmodule_list_itr iterator;
	unsigned count = 0;

	module_list_itr_init(&iterator);
	while (module_list_itr_next(&iterator) != NULL)
	{
		count++;
	}
	return count;
}

static void wait_for_child(void)
{
	uint64_t start = events_monotonic_ns();
	struct timespec pause = { 0, 10000000 };
	int status;

	assert(child_pid > 0 && start != UINT64_MAX);
	for (;;)
	{
		pid_t result = waitpid(child_pid, &status, WNOHANG);
		if (result == child_pid)
		{
			assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
			break;
		}
		/* The core's SIGCHLD handler may already have reaped this child. */
		if (result < 0 && errno == ECHILD)
		{
			break;
		}
		assert(result == 0 || (result < 0 && errno == EINTR));
		uint64_t now = events_monotonic_ns();
		assert(now != UINT64_MAX && now >= start &&
			now - start < UINT64_C(5000000000));
		nanosleep(&pause, NULL);
	}
}

static void check_argument_growth(const exec_context_t *exc)
{
	static const unsigned counts[] = { 0, 1, 7, 8, 9, 31, 64, 127, 256 };
	unsigned char before[FD_SETSIZE], after[FD_SETSIZE];
	unsigned initial = snapshot(before);
	char action[8192];

	for (unsigned listen_only = 0; listen_only < 2; listen_only++)
	{
		for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); c++)
		{
			expected_count = counts[c];
			size_t used = strlen("/usr/bin/true");
			strcpy(action, "/usr/bin/true");
			for (unsigned i = 0; i < expected_count; i++)
			{
				if (i == 0)
				{
					strcpy(expected_tokens[i], "LauncherAlias");
				}
				else
				{
					snprintf(expected_tokens[i], sizeof(expected_tokens[i]),
						"token %03u", i);
				}
				int added = snprintf(action + used, sizeof(action) - used,
					" \"%s\"", expected_tokens[i]);
				assert(added > 0 && (size_t)added < sizeof(action) - used);
				used += (size_t)added;
			}
			unsigned calls = fork_calls;
			argument_resizes = 0;
			argument_vector = NULL;
			argument_capacity = 0;
			measure_arguments = True;
			refuse_fork = True;
			if (listen_only)
			{
				CMD_ModuleListenOnly(NULL, exc, action, NULL);
			}
			else
			{
				CMD_Module(NULL, exc, action, NULL);
			}
			measure_arguments = False;
			refuse_fork = False;
			assert(fork_calls == calls + 1 && module_count() == 0);
			assert(snapshot(after) == initial);
			assert(memcmp(before, after, sizeof(before)) == 0);
#if EXPECT_LINEAR_GROWTH
			assert(argument_resizes == expected_count);
#else
			assert(argument_resizes <= 8);
			assert(expected_count != 0 || argument_resizes == 0);
#endif
			printf("module-arguments: mode=%u tokens=%u resizes=%u passed\n",
				listen_only, expected_count, argument_resizes);
		}
	}
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	unsigned char before[FD_SETSIZE], after[FD_SETSIZE];
	unsigned initial = snapshot(before);

	assert(module_count() == 0);
	check_ice_retry();
	check_argument_growth(exc);
	for (unsigned listen_only = 0; listen_only < 2; listen_only++)
	{
		for (unsigned attempt = 0; attempt < 3; attempt++)
		{
			unsigned calls = fork_calls;

			refuse_fork = True;
			if (listen_only)
			{
				CMD_ModuleListenOnly(NULL, exc, "/usr/bin/true", NULL);
			}
			else
			{
				CMD_Module(NULL, exc, "/usr/bin/true", NULL);
			}
			refuse_fork = False;
			assert(fork_calls == calls + 1 && module_count() == 0);
			assert(snapshot(after) == initial);
			assert(memcmp(before, after, sizeof(before)) == 0);
		}
		unsigned calls = fork_calls;
		child_pid = -1;
		if (listen_only)
		{
			CMD_ModuleListenOnly(NULL, exc, "/usr/bin/true", NULL);
		}
		else
		{
			CMD_Module(NULL, exc, "/usr/bin/true", NULL);
		}
		assert(fork_calls == calls + 1 && module_count() == 1);
		assert(snapshot(after) == initial + (listen_only ? 1 : 2));
		wait_for_child();
		module_kill_all();
		module_cleanup();
		assert(module_count() == 0 && snapshot(after) == initial);
		assert(memcmp(before, after, sizeof(before)) == 0);
	}
	puts("module-constructor-fork-refusal-and-descriptor-retry: passed");
	fflush(stdout);
	_exit(0);
}
