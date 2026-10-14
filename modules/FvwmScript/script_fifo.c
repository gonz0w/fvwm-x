/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "script_fifo.h"
#include "libs/fd_safety.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>

static int64_t monotonic_ms(void)
{
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
		return -1;
	if (now.tv_sec < 0 || (uint64_t)now.tv_sec >
	    (INT64_MAX - SCRIPT_FIFO_MAX_TIMEOUT_MS) / 1000)
	{
		errno = EOVERFLOW;
		return -1;
	}
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int remaining_ms(int64_t deadline, script_fifo_cancel_fn cancel,
	void *context)
{
	if (cancel != NULL && cancel(context))
	{
		errno = ECANCELED;
		return -1;
	}
	int64_t now = monotonic_ms();
	if (now < 0)
		return -1;
	if (now >= deadline)
	{
		errno = ETIMEDOUT;
		return -1;
	}
	return (int)(deadline - now);
}

static int wait_step(int fd, short events, int64_t deadline,
	script_fifo_cancel_fn cancel, void *context)
{
	int remaining = remaining_ms(deadline, cancel, context);
	if (remaining < 0)
		return -1;
	/* Bound startup retries and the signal-before-poll cancellation race.
	 * Readiness and interruptions never restart the overall deadline. */
	struct pollfd endpoint = {fd, events, 0};
	int result = poll(fd >= 0 ? &endpoint : NULL, fd >= 0 ? 1 : 0,
		remaining > 50 ? 50 : remaining);
	if (result < 0)
		return errno == EINTR ? 0 : -1;
	if (endpoint.revents & POLLNVAL)
	{
		errno = EBADF;
		return -1;
	}
	return result > 0 ? endpoint.revents : 0;
}

static int open_endpoint(int directory, const char *name, int mode)
{
	struct stat before, after;
	if (fstatat(directory, name, &before, AT_SYMLINK_NOFOLLOW) < 0)
		return -1;
	if (!S_ISFIFO(before.st_mode) || before.st_uid != getuid())
	{
		errno = EPERM;
		return -1;
	}
	int fd = openat(directory, name,
		mode | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
	if (fd < 0)
		return -1;
	if (fstat(fd, &after) < 0 || !fvwm_fd_set_cloexec(fd, true))
	{
		int error = errno;
		close(fd);
		errno = error;
		return -1;
	}
	if (!S_ISFIFO(after.st_mode) || after.st_uid != getuid() ||
	    before.st_dev != after.st_dev || before.st_ino != after.st_ino)
	{
		close(fd);
		errno = EPERM;
		return -1;
	}
	return fd;
}

static int await_endpoint(int directory, const char *name, int mode,
	bool immediate, int64_t deadline, script_fifo_cancel_fn cancel,
	void *context)
{
	for (;;)
	{
		if (remaining_ms(deadline, cancel, context) < 0)
			return -1;
		int fd = open_endpoint(directory, name, mode);
		if (fd >= 0 || immediate)
			return fd;
		if (errno != ENOENT && errno != ENXIO && errno != EINTR)
			return -1;
		if (wait_step(-1, 0, deadline, cancel, context) < 0)
			return -1;
	}
}


bool script_fifo_exchange(const char *directory, const char *id,
	const char *command, int mode, int timeout_ms,
	script_fifo_cancel_fn cancel, void *context, char **reply, size_t *length)
{
	int directory_fd = -1, endpoint = -1, error;
	char *name = NULL, *buffer = NULL;
	size_t used = 0;
	*reply = NULL;
	*length = 0;
	if (directory == NULL || id == NULL || *id == '\0' ||
	    strchr(id, '/') != NULL || command == NULL || mode < 0 || mode > 2 ||
	    timeout_ms < 1 || timeout_ms > SCRIPT_FIFO_MAX_TIMEOUT_MS)
	{
		errno = EINVAL;
		return false;
	}
	int64_t now = monotonic_ms();
	if (now < 0)
		return false;
	int64_t deadline = now + timeout_ms;
	directory_fd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (directory_fd < 0)
		return false;
	if (!fvwm_fd_set_cloexec(directory_fd, true))
		goto failed;
	size_t id_length = strlen(id);
	errno = 0;
	long name_max = fpathconf(directory_fd, _PC_NAME_MAX);
	if (name_max < 0 && errno != 0)
		goto failed;
	/* Both protocol names must fit the filesystem's actual component limit. */
	if (id_length > SIZE_MAX - sizeof(".tmp-com-out-") ||
	    (name_max >= 0 && id_length + strlen(".tmp-com-out-") >
	    (size_t)name_max))
	{
		errno = ENAMETOOLONG;
		goto failed;
	}
	name = malloc(id_length + sizeof(".tmp-com-out-"));
	if (name == NULL)
		goto failed;
	snprintf(name, id_length + sizeof(".tmp-com-out-"),
		".tmp-com-in-%s", id);
	endpoint = await_endpoint(directory_fd, name, O_WRONLY, mode == 2,
		deadline, cancel, context);
	if (endpoint < 0)
		goto failed;
	size_t command_length = strlen(command);
	if (mode == 2)
	{
		long atomic_size = fpathconf(endpoint, _PC_PIPE_BUF);
		if (atomic_size < 1 || command_length > (size_t)atomic_size)
		{
			errno = EMSGSIZE;
			goto failed;
		}
		ssize_t sent = fvwm_write_without_sigpipe(endpoint, command,
			command_length);
		if (sent < 0 || (size_t)sent != command_length)
		{
			if (sent >= 0)
				errno = EIO;
			goto failed;
		}
	}
	else
	{
		while (used < command_length)
		{
			if (remaining_ms(deadline, cancel, context) < 0)
				goto failed;
			ssize_t sent = fvwm_write_without_sigpipe(endpoint, command + used,
				command_length - used);
			if (sent > 0)
				used += (size_t)sent;
			else if (sent < 0 && (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK))
			{
				if (wait_step(endpoint, POLLOUT, deadline, cancel,
				    context) < 0)
					goto failed;
			}
			else
			{
				if (sent == 0)
					errno = EIO;
				goto failed;
			}
		}
	}
	close(endpoint);
	endpoint = -1;
	if (mode != 1)
	{
		free(name);
		close(directory_fd);
		return true;
	}
	snprintf(name, id_length + sizeof(".tmp-com-out-"),
		".tmp-com-out-%s", id);
	endpoint = await_endpoint(directory_fd, name, O_RDONLY, false,
		deadline, cancel, context);
	if (endpoint < 0)
		goto failed;
	buffer = malloc(SCRIPT_FIFO_REPLY_MAX_BYTES + 1);
	if (buffer == NULL)
		goto failed;
	used = 0;
	for (;;)
	{
		int ready = wait_step(endpoint, POLLIN, deadline, cancel, context);
		if (ready < 0)
			goto failed;
		if (ready == 0)
			continue;
		ssize_t count = read(endpoint, buffer + used,
			SCRIPT_FIFO_REPLY_MAX_BYTES - used);
		if (count < 0)
		{
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			goto failed;
		}
		used += (size_t)count;
		if (count == 0 || used == SCRIPT_FIFO_REPLY_MAX_BYTES)
			break;
	}
	buffer[used] = '\0';
	if (memchr(buffer, '\0', used) != NULL)
	{
		errno = EINVAL;
		goto failed;
	}
	close(endpoint);
	close(directory_fd);
	free(name);
	*reply = buffer;
	*length = used;
	return true;
failed:
	error = errno;
	if (endpoint >= 0)
		close(endpoint);
	if (directory_fd >= 0)
		close(directory_fd);
	free(name);
	free(buffer);
	errno = error;
	return false;
}
