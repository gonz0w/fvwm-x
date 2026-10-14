/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/fd_safety.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>

#if defined(__GLIBC__) && defined(__USE_FILE_OFFSET64)
#define __wrap_fcntl __wrap_fcntl64
#define __real_fcntl __real_fcntl64
#endif

static bool policy_mode;
static unsigned calls, fail_call;
static int received_flags;
int __real_fcntl(int descriptor, int command, ...);

int __wrap_fcntl(int descriptor, int command, ...)
{
	int flags = 0;

	assert(command == F_GETFD || command == F_SETFD);
	if (command == F_SETFD)
	{
		va_list arguments;
		va_start(arguments, command);
		flags = va_arg(arguments, int);
		va_end(arguments);
	}
	if (!policy_mode)
	{
		return __real_fcntl(descriptor, command, flags);
	}
	if (++calls == fail_call)
	{
		errno = EIO;
		return -1;
	}
	if (command == F_SETFD)
	{
		received_flags = flags;
	}
	return 0;
}

static void test_range_and_flag_failures(void)
{
	static const int invalid[] = { -1, INT_MIN, FD_SETSIZE, INT_MAX };
	static const int valid[] = { 0, 1, FD_SETSIZE - 1 };

	policy_mode = true;
	for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		calls = 0;
		errno = 0;
		assert(!fvwm_fd_prepare_select(invalid[i]));
		assert(errno == EINVAL && calls == 0);
	}
	for (unsigned i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
	{
		calls = 0;
		received_flags = 0;
		assert(fvwm_fd_prepare_select(valid[i]));
		assert(calls == 2 && received_flags == FD_CLOEXEC);
	}
	for (fail_call = 1; fail_call <= 2; fail_call++)
	{
		calls = 0;
		assert(!fvwm_fd_prepare_select(1));
		assert(errno == EIO && calls == fail_call);
	}
	fail_call = 0;
	calls = 0;
	/* Generic inheritance protection also serves poll-based owners. */
	assert(fvwm_fd_set_cloexec(FD_SETSIZE + 7, true) && calls == 2);
	policy_mode = false;
}

static void test_owned_pipe(void)
{
	int descriptors[2];

	assert(fvwm_pipe_cloexec(descriptors));
	for (unsigned i = 0; i < 2; i++)
	{
		assert(fvwm_fd_set_cloexec(descriptors[i], false));
		assert(fvwm_fd_prepare_select(descriptors[i]));
		assert(fcntl(descriptors[i], F_GETFD) & FD_CLOEXEC);
		assert(close(descriptors[i]) == 0);
	}
}

int main(void)
{
	test_range_and_flag_failures();
	test_owned_pipe();
	puts("select descriptor admission: range, flags and owned pipe pass");
	return 0;
}
