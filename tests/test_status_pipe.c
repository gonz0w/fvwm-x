/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "fvwm/status_pipe.h"
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

/* Match glibc's production large-file redirect; BSD uses open directly. */
#if defined(__GLIBC__) && defined(__USE_FILE_OFFSET64)
#define __wrap_open __wrap_open64
#define __real_open __real_open64
#endif

static int fault, opened_fd = -1, writes;
int __real_open(const char *, int, ...);
ssize_t __real_write(int, const void *, size_t);
bool __real_fvwm_fd_set_cloexec(int, bool);

int
__wrap_open(const char *path, int flags, ...)
{
	mode_t mode = 0;

	if (flags & O_CREAT)
	{
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, int);
		va_end(args);
	}
	if (fault == 1)
	{
		errno = EACCES;
		return -1;
	}
	if (fault == 3)
	{
		assert(unlink(path) == 0);
		int replacement = __real_open(path, O_CREAT | O_WRONLY, 0600);
		assert(replacement >= 0);
		close(replacement);
	}
	opened_fd = __real_open(path, flags, mode);
	return opened_fd;
}

/* Fortified glibc/Clang calls use these entry points. Inject at the same
 * owner boundary without weakening the production compilation flags. */
int
__wrap___open_2(const char *path, int flags)
{
	return __wrap_open(path, flags);
}

int
__wrap___open64_2(const char *path, int flags)
{
	return __wrap_open(path, flags);
}

bool
__wrap_fvwm_fd_set_cloexec(int fd, bool enabled)
{
	if (fault == 2)
	{
		errno = EIO;
		return false;
	}
	return __real_fvwm_fd_set_cloexec(fd, enabled);
}

ssize_t
__wrap_write(int fd, const void *bytes, size_t length)
{
	writes++;
	if (fault == 4 || fault == 5)
	{
		errno = fault == 4 ? EINTR : EIO;
		return -1;
	}
	if (fault == 6)
	{
		return 0;
	}
	return __real_write(fd, bytes, length);
}

int
main(int argc, char **argv)
{
	if (argc == 2)
	{
		assert(fcntl(atoi(argv[1]), F_GETFD) < 0 && errno == EBADF);
		return 0;
	}
	char directory[] = "/tmp/fvwm-status-pipe-XXXXXX";
	char path[256], target[256];
	struct stat before, after;
	fvwm_status_pipe pipe = FVWM_STATUS_PIPE_INIT;

	assert(mkdtemp(directory) != NULL);
	snprintf(path, sizeof(path), "%s/fvwm3.pipe", directory);
	snprintf(target, sizeof(target), "%s/keep", directory);
	int fd = open(target, O_CREAT | O_WRONLY, 0600);
	assert(fd >= 0);
	assert(write(fd, "keep", 4) == 4);
	close(fd);
	assert(symlink(target, path) == 0);
	assert(lstat(path, &before) == 0);
	assert(!status_pipe_open(&pipe, directory) && errno == EEXIST);
	assert(lstat(path, &after) == 0 && before.st_ino == after.st_ino);
	assert(stat(target, &after) == 0 && after.st_size == 4);
	assert(unlink(path) == 0);
	assert(rename(target, path) == 0);
	assert(!status_pipe_open(&pipe, directory) && errno == EEXIST);
	assert(stat(path, &after) == 0 && after.st_size == 4);
	assert(unlink(path) == 0);
	assert(mkfifo(path, 0600) == 0);
	assert(!status_pipe_open(&pipe, directory) && errno == EEXIST);
	assert(unlink(path) == 0);

	for (fault = 1; fault <= 3; fault++)
	{
		assert(!status_pipe_open(&pipe, directory));
		assert(pipe.fd == -1 && pipe.path == NULL);
		if (fault != 1)
		{
			assert(fcntl(opened_fd, F_GETFD) < 0 && errno == EBADF);
		}
		if (fault == 3)
		{
			assert(stat(path, &after) == 0);
			assert(S_ISREG(after.st_mode));
			assert(unlink(path) == 0);
		}
		else
		{
			assert(lstat(path, &after) < 0 &&
			    errno == ENOENT);
		}
	}
	fault = 0;
	assert(status_pipe_open(&pipe, directory));
	assert(fcntl(pipe.fd, F_GETFD) & FD_CLOEXEC);
	char number[32];
	snprintf(number, sizeof(number), "%d", pipe.fd);
	pid_t child = fork();
	assert(child >= 0);
	if (child == 0)
	{
		execl(argv[0], argv[0], number, (char *)NULL);
		_exit(127);
	}
	int child_status;
	assert(waitpid(child, &child_status, 0) == child);
	assert(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
	assert(fcntl(pipe.fd, F_GETFL) & O_NONBLOCK);
	assert(fstat(pipe.fd, &after) == 0 && (after.st_mode & 077) == 0);
	assert(!status_pipe_open(&pipe, directory) && errno == EBUSY);
	assert(!status_pipe_queue(&pipe, "", (size_t)-1));
	assert(errno == EMSGSIZE && pipe.pending == NULL);

	/* Force real backpressure with a record larger than the pipe. Drain
	 * independently and require exact bytes, then a separate record. */
	size_t length = FVWM_STATUS_MAX_RECORD - 1, received = 0;
	char *text = malloc(length + 1), *result = malloc(length + 2);
	assert(text != NULL && result != NULL);
	memset(text, 'x', length);
	text[length] = '\0';
	assert(status_pipe_queue(&pipe, text, length));
	assert(!status_pipe_queue(&pipe, "{}", 2));
	assert(errno == EAGAIN);
	assert(status_pipe_flush(&pipe) == 0 && pipe.offset > 0);
	assert(pipe.length == FVWM_STATUS_MAX_RECORD);
	while (pipe.pending != NULL || received < length + 1)
	{
		ssize_t count = read(pipe.fd, result + received,
		    length + 1 - received);
		if (count > 0)
		{
			received += (size_t)count;
		}
		else
		{
			assert(count < 0 &&
			    (errno == EAGAIN || errno == EWOULDBLOCK));
		}
		assert(status_pipe_flush(&pipe) >= 0);
	}
	assert(memcmp(result, text, length) == 0 && result[length] == '\n');
	assert(status_pipe_queue(&pipe, "{}", 2));
	fault = 4;
	writes = 0;
	assert(status_pipe_flush(&pipe) == 0 && writes == 4);
	assert(pipe.offset == 0);
	fault = 0;
	assert(status_pipe_flush(&pipe) == 1);
	assert(read(pipe.fd, result, length + 2) == 3);
	assert(memcmp(result, "{}\n", 3) == 0);
	for (int kind = 5; kind <= 6; kind++)
	{
		fault = kind;
		assert(status_pipe_queue(&pipe, "{}", 2));
		assert(status_pipe_flush(&pipe) == -1 && errno == EIO);
		status_pipe_close(&pipe);
		assert(pipe.fd == -1 && pipe.pending == NULL);
		fault = 0;
		assert(status_pipe_open(&pipe, directory));
	}
	fault = 0;
	fd = pipe.fd;
	assert(unlink(path) == 0);
	int replacement = open(path, O_CREAT | O_WRONLY, 0600);
	assert(replacement >= 0);
	close(replacement);
	status_pipe_close(&pipe);
	assert(fcntl(fd, F_GETFD) < 0 && errno == EBADF);
	assert(stat(path, &after) == 0 && S_ISREG(after.st_mode));
	status_pipe_close(&pipe);
	assert(unlink(path) == 0 && rmdir(directory) == 0);
	free(text);
	free(result);
	puts("status pipe: collision, rollback, flags, bounds, "
	    "continuation, retirement");
	return 0;
}
