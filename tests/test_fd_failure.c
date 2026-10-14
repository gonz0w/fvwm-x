/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/fd_safety.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* Meson enables large-file redirects on glibc. Inject into the symbols
 * actually called by the production object; BSD and musl use POSIX names. */
#if defined(__GLIBC__) && defined(__USE_FILE_OFFSET64)
#define __wrap_fcntl __wrap_fcntl64
#define __real_fcntl __real_fcntl64
#define __wrap_fstat __wrap_fstat64
#define __real_fstat __real_fstat64
#endif

static int fail_pipe, fail_fcntl_call, fcntl_calls, fail_stat;
static int opened[2] = {-1, -1};
static int closed[2], close_count;

int __real_pipe(int descriptors[2]);
int __real_fcntl(int descriptor, int command, ...);
int __real_fstat(int descriptor, struct stat *status);
int __real_close(int descriptor);

int
__wrap_pipe(int descriptors[2])
{
	if (fail_pipe)
	{
		errno = EMFILE;
		return -1;
	}
	int result = __real_pipe(descriptors);
	if (result == 0)
	{
		opened[0] = descriptors[0];
		opened[1] = descriptors[1];
	}
	return result;
}

int
__wrap_fcntl(int descriptor, int command, ...)
{
	assert(command == F_GETFD || command == F_SETFD);
	int argument = 0;
	if (command == F_SETFD)
	{
		va_list args;
		va_start(args, command);
		argument = va_arg(args, int);
		va_end(args);
	}
	if (++fcntl_calls == fail_fcntl_call)
	{
		errno = EIO;
		return -1;
	}
	return __real_fcntl(descriptor, command, argument);
}

int
__wrap_fstat(int descriptor, struct stat *status)
{
	if (fail_stat)
	{
		errno = EIO;
		return -1;
	}
	return __real_fstat(descriptor, status);
}

int
__wrap_close(int descriptor)
{
	assert(close_count < 2);
	closed[close_count++] = descriptor;
	int result = __real_close(descriptor);
	/* Successful teardown may change errno. The setup error must win. */
	errno = EBADF;
	return result;
}

static void
test_pipe_failure_outputs_and_cleanup(void)
{
	int descriptors[2] = {99, 100};
	fail_pipe = 1;
	assert(!fvwm_pipe_cloexec(descriptors));
	assert(errno == EMFILE && descriptors[0] == -1 && descriptors[1] == -1);
	assert(close_count == 0);
	fail_pipe = 0;
	for (int failure = 1; failure <= 4; failure++)
	{
		fcntl_calls = close_count = 0;
		fail_fcntl_call = failure;
		assert(!fvwm_pipe_cloexec(descriptors));
		assert(errno == EIO && descriptors[0] == -1 &&
		    descriptors[1] == -1);
		assert(close_count == 2 && closed[0] == opened[0] &&
		    closed[1] == opened[1]);
		for (int i = 0; i < 2; i++)
		{
			assert(__real_fcntl(opened[i], F_GETFD) == -1 &&
			    errno == EBADF);
		}
	}
	fail_fcntl_call = 0;
}

static void
test_lock_stat_failure_closes_descriptor(void)
{
	char directory[] = "/tmp/fvwm-fd-failure-XXXXXX";
	char path[256];
	assert(mkdtemp(directory) != NULL);
	snprintf(path, sizeof(path), "%s/lock", directory);
	close_count = 0;
	fail_stat = 1;
	assert(fvwm_open_private_lock(path) == -1 && errno == EIO);
	assert(close_count == 1);
	assert(__real_fcntl(closed[0], F_GETFD) == -1 && errno == EBADF);
	fail_stat = 0;
	assert(unlink(path) == 0 && rmdir(directory) == 0);
}

int
main(void)
{
	test_pipe_failure_outputs_and_cleanup();
	test_lock_stat_failure_closes_descriptor();
	puts("descriptor failures reset outputs, close handles and keep errno");
	return 0;
}
