/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/Module.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int fail_call;
static int calls;

/* Meson redirects glibc large-file calls; BSD/musl use the POSIX symbol. */
#if defined(__GLIBC__) && defined(__USE_FILE_OFFSET64)
#define __wrap_fcntl __wrap_fcntl64
#define __real_fcntl __real_fcntl64
#endif

int __real_fcntl(int descriptor, int command, ...);

int __wrap_fcntl(int descriptor, int command, ...)
{
	va_list args;
	int value = 0;

	assert(command == F_GETFD || command == F_SETFD || command == F_DUPFD);
	va_start(args, command);
	if (command == F_SETFD || command == F_DUPFD)
		value = va_arg(args, int);
	va_end(args);
	if (fail_call != 0 && ++calls == fail_call)
	{
		errno = EIO;
		return -1;
	}
	return __real_fcntl(descriptor, command, value);
}

static ModuleArgs *parse_pair(int descriptors[2], const char *alias)
{
	char first[32], second[32];
	char *args[] = {"/modules/Probe", first, second, "none",
		"abcd", "12", (char *)alias, "user", NULL};

	snprintf(first, sizeof(first), "%d", descriptors[0]);
	snprintf(second, sizeof(second), "%d", descriptors[1]);
	return ParseModuleArgs(alias == NULL ? 6 : 8, args, alias != NULL);
}

static void test_valid_and_exec(const char *executable)
{
	int descriptors[2], status;
	char first[32], second[32], value;

	assert(pipe(descriptors) == 0);
	ModuleArgs *module = parse_pair(descriptors, NULL);
	assert(module != NULL && strcmp(module->name, "Probe") == 0);
	assert(module->user_argc == 0 && module->user_argv == NULL);
	assert(module->window == 0xabcd && module->decoration == 0x12);
	assert(module->to_fvwm == descriptors[0]);
	assert(module->from_fvwm == descriptors[1]);
	module = parse_pair(descriptors, "Alias");
	assert(module != NULL && strcmp(module->name, "Alias") == 0);
	assert(module->user_argc == 1);
	/* user_argv borrows the argument array; do not retain this test's
	 * stack argument vector after parse_pair returns. */
	assert(fcntl(descriptors[0], F_GETFD) & FD_CLOEXEC);
	assert(fcntl(descriptors[1], F_GETFD) & FD_CLOEXEC);
	assert(write(descriptors[1], "x", 1) == 1);
	assert(read(descriptors[0], &value, 1) == 1 && value == 'x');
	snprintf(first, sizeof(first), "%d", descriptors[0]);
	snprintf(second, sizeof(second), "%d", descriptors[1]);
	pid_t child = fork();
	assert(child >= 0);
	if (child == 0)
	{
		execl(executable, executable, "--child", first, second,
			(char *)NULL);
		_exit(127);
	}
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	assert(fcntl(descriptors[0], F_GETFD) >= 0);
	assert(fcntl(descriptors[1], F_GETFD) >= 0);
	close(descriptors[0]);
	close(descriptors[1]);
}

static void test_invalid_arguments(void)
{
	int descriptors[2];
	char valid[32], other[32], overflow[64];
	const char *invalid[] = {"", "-1", "+3", " 3", "3 ", "3x",
		"1.0", "9999999999999999999999999999999999999999999"};
	char *args[] = {"Probe", valid, other, "none", "0", "0", NULL};

	assert(pipe(descriptors) == 0);
	snprintf(valid, sizeof(valid), "%d", descriptors[0]);
	snprintf(other, sizeof(other), "%d", descriptors[1]);
	assert(ParseModuleArgs(5, args, 0) == NULL);
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		args[1] = (char *)invalid[i];
		assert(ParseModuleArgs(6, args, 0) == NULL);
		args[1] = valid;
		args[2] = (char *)invalid[i];
		if (strcmp(invalid[i], "-1") != 0)
			assert(ParseModuleArgs(6, args, 0) == NULL);
		args[2] = other;
	}
	snprintf(overflow, sizeof(overflow), "%lu", (unsigned long)INT_MAX + 1);
	args[1] = overflow;
	assert(ParseModuleArgs(6, args, 0) == NULL);
	args[1] = valid;
	args[2] = valid;
	assert(ParseModuleArgs(6, args, 0) == NULL);
	args[2] = other;
	close(descriptors[1]);
	assert(ParseModuleArgs(6, args, 0) == NULL && errno == EBADF);
	assert(fcntl(descriptors[0], F_GETFD) >= 0);
	close(descriptors[0]);
}

static void test_listen_only(void)
{
	int descriptors[2];

	assert(pipe(descriptors) == 0);
	int listen[2] = {descriptors[1], -1};
	ModuleArgs *module = parse_pair(listen, NULL);
	assert(module != NULL && module->from_fvwm == -1);
	assert(module->to_fvwm == descriptors[1]);
	assert(fcntl(descriptors[1], F_GETFD) & FD_CLOEXEC);
	assert((fcntl(descriptors[0], F_GETFD) & FD_CLOEXEC) == 0);
	close(descriptors[0]);
	close(descriptors[1]);
}

static void test_flag_failures(void)
{
	for (int failure = 1; failure <= 4; failure++)
	{
		int descriptors[2];

		assert(pipe(descriptors) == 0);
		fail_call = failure;
		calls = 0;
		assert(parse_pair(descriptors, NULL) == NULL && errno == EIO);
		fail_call = 0;
		/* Rejected startup never closes caller-owned endpoints. */
		assert(fcntl(descriptors[0], F_GETFD) >= 0);
		assert(fcntl(descriptors[1], F_GETFD) >= 0);
		close(descriptors[0]);
		close(descriptors[1]);
	}
}

static void test_high_descriptors(void)
{
	int descriptors[2];

	assert(pipe(descriptors) == 0);
	int high = fcntl(descriptors[0], F_DUPFD, 1025);
	if (high >= 0)
	{
		close(descriptors[0]);
		descriptors[0] = high;
		assert(parse_pair(descriptors, NULL) != NULL);
		assert(fcntl(high, F_GETFD) & FD_CLOEXEC);
	}
	close(descriptors[0]);
	close(descriptors[1]);
}

int main(int argc, char **argv)
{
	if (argc == 4 && strcmp(argv[1], "--child") == 0)
	{
		assert(fcntl(atoi(argv[2]), F_GETFD) == -1 && errno == EBADF);
		assert(fcntl(atoi(argv[3]), F_GETFD) == -1 && errno == EBADF);
		return 0;
	}
	test_valid_and_exec(argv[0]);
	test_invalid_arguments();
	test_listen_only();
	test_flag_failures();
	test_high_descriptors();
	return 0;
}
