/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "fd_safety.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

bool
fvwm_fd_set_cloexec(int descriptor, bool enabled)
{
	int flags = fcntl(descriptor, F_GETFD);

	return flags >= 0 && fcntl(descriptor, F_SETFD,
	    enabled ? flags | FD_CLOEXEC : flags & ~FD_CLOEXEC) == 0;
}

bool
fvwm_fd_prepare_select(int descriptor)
{
	if (descriptor < 0 || descriptor >= FD_SETSIZE)
	{
		errno = EINVAL;
		return false;
	}
	return fvwm_fd_set_cloexec(descriptor, true);
}

bool
fvwm_pipe_cloexec(int descriptors[2])
{
	descriptors[0] = descriptors[1] = -1;
	if (pipe(descriptors) != 0)
	{
		return false;
	}
	/* The core creates children only on its owning thread; signal handlers
	 * defer work, so no fork can run between pipe() and these flag updates. */
	if (!fvwm_fd_set_cloexec(descriptors[0], true) ||
	    !fvwm_fd_set_cloexec(descriptors[1], true))
	{
		int saved_errno = errno;
		close(descriptors[0]);
		close(descriptors[1]);
		descriptors[0] = descriptors[1] = -1;
		errno = saved_errno;
		return false;
	}
	return true;
}

int
fvwm_open_private_lock(const char *path)
{
	struct stat status;
	int descriptor = open(path,
	    O_CREAT | O_WRONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK, 0600);

	if (descriptor < 0)
	{
		return -1;
	}
	if (fstat(descriptor, &status) != 0)
	{
		int saved_errno = errno;
		close(descriptor);
		errno = saved_errno;
		return -1;
	}
	if (!S_ISREG(status.st_mode) || status.st_uid != getuid() ||
	    (status.st_mode & 077) != 0 || status.st_nlink != 1)
	{
		close(descriptor);
		errno = EPERM;
		return -1;
	}
	return descriptor;
}

ssize_t
fvwm_write_without_sigpipe(int fd, const void *text, size_t length)
{
	sigset_t blocked, previous, pending;
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGPIPE);
	if (sigprocmask(SIG_BLOCK, &blocked, &previous) < 0)
	{
		return -1;
	}
	if (sigpending(&pending) < 0)
	{
		int error = errno;
		sigprocmask(SIG_SETMASK, &previous, NULL);
		errno = error;
		return -1;
	}
	bool already_pending = sigismember(&pending, SIGPIPE) == 1;
	ssize_t result = write(fd, text, length);
	int error = errno;
	/* Consume only a newly pending broken-pipe signal from this write;
	 * preserve the caller's dispositions, mask and preexisting pending signal. */
	if (result < 0 && error == EPIPE && !already_pending &&
	    sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 1)
	{
		int received;
		int status = sigwait(&blocked, &received);
		if (status != 0)
		{
			error = status;
		}
	}
	if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0)
	{
		return -1;
	}
	errno = error;
	return result;
}
