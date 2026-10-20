/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/execcontext.h"
#include "fvwm/commands.h"
#include "libs/setpgrp.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__GLIBC__) && defined(__USE_FILE_OFFSET64)
#define __wrap_open __wrap_open64
#define __real_open __real_open64
#endif

enum fault { NORMAL, OPEN_FAIL, DUP_FAIL, GROUP_FAIL, EXEC_FAIL, FORK_FAIL };
static enum fault fault;
static pid_t owner, child;
static unsigned diagnostics;
static int callbacks[2];

pid_t __real_fork(void);
pid_t __wrap_fork(void)
{
	if (fault == FORK_FAIL)
	{
		errno = EAGAIN;
		return -1;
	}
	child = __real_fork();
	return child;
}

int __real_open(const char *, int, ...);
int __wrap_open(const char *path, int flags, ...)
{
	assert(strcmp(path, "/dev/null") == 0 && !(flags & O_CREAT));
	if (fault == OPEN_FAIL)
	{
		errno = EMFILE;
		return -1;
	}
	return __real_open(path, flags);
}

int __real_dup2(int, int);
int __wrap_dup2(int from, int to)
{
	assert(to == STDIN_FILENO);
	if (fault == DUP_FAIL)
	{
		errno = EBADF;
		return -1;
	}
	return __real_dup2(from, to);
}

int __real_fvwm_setpgrp(void);
int __wrap_fvwm_setpgrp(void)
{
	if (fault == GROUP_FAIL)
	{
		errno = EPERM;
		return -1;
	}
	return __real_fvwm_setpgrp();
}

int __real_execl(const char *, const char *, ...);
int __wrap_execl(const char *shell, const char *first, ...)
{
	va_list args;
	const char *option, *command;
	struct stat input, null_device;
	int fd = __real_open("/dev/null", O_RDONLY);

	assert(fd >= 0 && fstat(fd, &null_device) == 0);
	assert(fstat(STDIN_FILENO, &input) == 0);
	assert(S_ISCHR(input.st_mode) && input.st_rdev == null_device.st_rdev);
	assert(!(fcntl(STDIN_FILENO, F_GETFD) & FD_CLOEXEC));
	assert(getpgrp() == getpid());
	close(fd);
	/* An unchecked earlier failure must never reach exec. */
	assert(fault == NORMAL || fault == EXEC_FAIL);
	if (fault == EXEC_FAIL)
	{
		errno = ENOENT;
		return -1;
	}
	va_start(args, first);
	option = va_arg(args, const char *);
	command = va_arg(args, const char *);
	assert(va_arg(args, char *) == NULL);
	va_end(args);
	assert(strcmp(option, "-c") == 0);
	return __real_execl(shell, first, option, command, (char *)NULL);
}

void __wrap_fvwm_debug(const char *function, const char *format, ...)
{
	(void)function;
	(void)format;
	diagnostics++;
}

static void inherited_callback(void)
{
	if (getpid() != owner)
		assert(write(callbacks[1], "x", 1) == 1);
}

static void observe(enum fault mode, int closed_stdin)
{
	FILE *buffered = tmpfile();
	struct stat status;
	int saved = -1;

	assert(buffered != NULL);
	assert(fputs("x", buffered) >= 0);
	if (closed_stdin)
	{
		saved = dup(STDIN_FILENO);
		assert(saved >= 0 && close(STDIN_FILENO) == 0);
	}
	fault = mode;
	child = -1;
	CMD_Exec(NULL, NULL, "exit 0", NULL);
	if (closed_stdin)
	{
		assert(__real_dup2(saved, STDIN_FILENO) == STDIN_FILENO);
		close(saved);
	}
	if (mode == FORK_FAIL)
	{
		assert(child == -1 && diagnostics == 1);
	}
	else
	{
		int result;
		assert(child > 0 && waitpid(child, &result, 0) == child);
		assert(WIFEXITED(result));
		assert(WEXITSTATUS(result) == (mode == NORMAL ? 0 : 100));
	}
	/* exit() in a failed child would flush the parent's buffered byte and
	 * run its callback. The parent still owns its original buffer. */
	assert(fstat(fileno(buffered), &status) == 0 && status.st_size == 0);
	assert(fflush(buffered) == 0);
	assert(fstat(fileno(buffered), &status) == 0 && status.st_size == 1);
	assert(fclose(buffered) == 0);
	char byte;
	assert(read(callbacks[0], &byte, 1) == -1 && errno == EAGAIN);
	printf("exec-startup mode=%d closed-stdin=%d: passed\n", mode,
		closed_stdin);
}

int __wrap_main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	owner = getpid();
	assert(pipe(callbacks) == 0);
	assert(fcntl(callbacks[0], F_SETFL, O_NONBLOCK) == 0);
	assert(atexit(inherited_callback) == 0);
	for (enum fault i = NORMAL; i <= FORK_FAIL; i++)
		observe(i, 0);
	observe(NORMAL, 1);
	close(callbacks[0]);
	close(callbacks[1]);
	return 0;
}
