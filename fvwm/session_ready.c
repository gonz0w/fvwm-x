/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "session_ready.h"
#include "libs/fd_safety.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

int session_ready_take(void)
{
	const char *value = getenv("FVWM_SESSION_READY_FD");
	int requested = value != NULL && strcmp(value, "3") == 0;
	struct stat info;
	int flags;

	unsetenv("FVWM_SESSION_READY_FD");
	if (!requested)
	{
		return -1;
	}
	flags = fcntl(3, F_GETFL);
	/* FreeBSD pipe ends can be O_RDWR; require write access, not Linux's
	 * unidirectional access-mode encoding. */
	if (flags < 0 || ((flags & O_ACCMODE) != O_WRONLY &&
	    (flags & O_ACCMODE) != O_RDWR) ||
	    !(flags & O_NONBLOCK) || fstat(3, &info) != 0 ||
	    !S_ISFIFO(info.st_mode) || !fvwm_fd_set_cloexec(3, true))
	{
		return -1;
	}
	return 3;
}

int session_lease_take(void)
{
	const char *value = getenv("FVWM_SESSION_LEASE_FD");
	int requested = value != NULL && strcmp(value, "4") == 0;
	struct stat info;
	int flags;

	unsetenv("FVWM_SESSION_LEASE_FD");
	if (!requested)
	{
		return -1;
	}
	flags = fcntl(4, F_GETFL);
	if (flags < 0 || (flags & O_ACCMODE) != O_RDWR ||
	    !(flags & O_NONBLOCK) || fstat(4, &info) != 0 ||
	    !S_ISREG(info.st_mode) || info.st_uid != getuid() ||
	    (info.st_mode & 077) != 0 || info.st_nlink != 1 ||
	    !fvwm_fd_set_cloexec(4, true) ||
	    flock(4, LOCK_EX | LOCK_NB) != 0)
	{
		return -1;
	}
	/* flock is shared by the inherited open-file description. Closing either
	 * owner's descriptor must not explicitly unlock the remaining owner. */
	return 4;
}

bool session_lease_prepare_restart(int descriptor)
{
	unsetenv("FVWM_SESSION_LEASE_FD");
	if (descriptor < 0)
	{
		return true;
	}
	if (setenv("FVWM_SESSION_LEASE_FD", "4", 1) != 0 ||
	    !fvwm_fd_set_cloexec(descriptor, false))
	{
		session_lease_finish_restart(descriptor);
		return false;
	}
	return true;
}

void session_lease_finish_restart(int descriptor)
{
	int saved_errno = errno;

	unsetenv("FVWM_SESSION_LEASE_FD");
	if (descriptor >= 0)
	{
		(void)fvwm_fd_set_cloexec(descriptor, true);
	}
	errno = saved_errno;
}

void session_ready_publish(int *descriptor, int lease_descriptor)
{
	if (*descriptor >= 0)
	{
		/* A full pipe or missing reader cannot stall/terminate the X owner. */
		(void)fvwm_write_without_sigpipe(
			*descriptor, "RL", lease_descriptor >= 0 ? 2 : 1);
		close(*descriptor);
		*descriptor = -1;
	}
}
