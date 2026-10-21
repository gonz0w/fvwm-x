/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "status_pipe.h"
#include "libs/fd_safety.h"
#include "libs/fvwmlib.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool
status_pipe_matches(const fvwm_status_pipe *pipe)
{
	struct stat status;

	return pipe->path != NULL && lstat(pipe->path, &status) == 0 &&
	    S_ISFIFO(status.st_mode) && status.st_dev == pipe->device &&
	    status.st_ino == pipe->inode && status.st_uid == getuid();
}

void
status_pipe_close(fvwm_status_pipe *pipe)
{
	/* Pin the inode with the descriptor until after the identity check. */
	if (status_pipe_matches(pipe))
	{
		unlink(pipe->path);
	}
	if (pipe->fd >= 0)
	{
		close(pipe->fd);
	}
	free(pipe->path);
	free(pipe->pending);
	*pipe = (fvwm_status_pipe)FVWM_STATUS_PIPE_INIT;
}

bool
status_pipe_open(fvwm_status_pipe *pipe, const char *directory)
{
	fvwm_status_pipe candidate = FVWM_STATUS_PIPE_INIT;
	struct stat created, opened;
	int error;

	if (pipe->fd >= 0 || pipe->path != NULL)
	{
		errno = EBUSY;
		return false;
	}
	if (directory == NULL || directory[0] == '\0')
	{
		errno = EINVAL;
		return false;
	}
	xasprintf(&candidate.path, "%s/fvwm3.pipe", directory);
	/* Refuse collisions, including another live session or a stale FIFO.
	 * Unlinking an existing pathname can delete unrelated user data. */
	if (mkfifo(candidate.path, 0600) != 0)
	{
		free(candidate.path);
		return false;
	}
	if (lstat(candidate.path, &created) != 0)
	{
		goto fail;
	}
	if (!S_ISFIFO(created.st_mode) || created.st_uid != getuid() ||
	    (created.st_mode & 077) != 0 || created.st_nlink != 1)
	{
		errno = EPERM;
		goto fail;
	}
	candidate.device = created.st_dev;
	candidate.inode = created.st_ino;
	candidate.fd = open(candidate.path,
	    O_RDWR | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
	if (candidate.fd < 0 || fstat(candidate.fd, &opened) != 0)
	{
		goto fail;
	}
	if (!S_ISFIFO(opened.st_mode) || opened.st_dev != created.st_dev ||
	    opened.st_ino != created.st_ino || opened.st_uid != getuid() ||
	    (opened.st_mode & 077) != 0 || opened.st_nlink != 1 ||
	    !status_pipe_matches(&candidate))
	{
		errno = EPERM;
		goto fail;
	}
	if (!fvwm_fd_set_cloexec(candidate.fd, true))
	{
		goto fail;
	}
	*pipe = candidate;
	return true;

fail:
	error = errno;
	status_pipe_close(&candidate);
	errno = error;
	return false;
}

bool
status_pipe_queue(fvwm_status_pipe *pipe, const char *text, size_t length)
{
	if (pipe->fd < 0 || text == NULL)
	{
		errno = EINVAL;
		return false;
	}
	if (pipe->pending != NULL)
	{
		errno = EAGAIN;
		return false;
	}
	if (length >= FVWM_STATUS_MAX_RECORD)
	{
		errno = EMSGSIZE;
		return false;
	}
	pipe->pending = fxmalloc(length + 1);
	memcpy(pipe->pending, text, length);
	pipe->pending[length] = '\n';
	pipe->length = length + 1;
	pipe->offset = 0;
	return true;
}

int
status_pipe_flush(fvwm_status_pipe *pipe)
{
	/* Bound both successful partial writes and interruptions. A slow reader
	 * retains one continuation; writable readiness resumes that record
	 * before another is queued, so JSON lines cannot be interleaved. */
	for (unsigned int calls = 0;
	    pipe->pending != NULL && calls < 4; calls++)
	{
		ssize_t count = write(pipe->fd, pipe->pending + pipe->offset,
		    pipe->length - pipe->offset);

		if (count > 0)
		{
			pipe->offset += (size_t)count;
			if (pipe->offset == pipe->length)
			{
				free(pipe->pending);
				pipe->pending = NULL;
				pipe->length = pipe->offset = 0;
			}
		}
		else if (count < 0 && errno == EINTR)
		{
			continue;
		}
		else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			return 0;
		}
		else
		{
			if (count == 0)
			{
				errno = EIO;
			}
			return -1;
		}
	}
	return pipe->pending == NULL ? 1 : 0;
}
