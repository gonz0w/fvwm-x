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

int session_bus_lifetime_take(void)
{
	const char *value = getenv("FVWM_SESSION_BUS_LIFETIME_FD");
	int requested = value != NULL && strcmp(value, "5") == 0;
	struct stat info;
	int flags;

	unsetenv("FVWM_SESSION_BUS_LIFETIME_FD");
	if (!requested)
		return -1;
	flags = fcntl(5, F_GETFL);
	/* BSD pipe endpoints may be O_RDWR; either writable mode is valid. */
	if (flags < 0 || ((flags & O_ACCMODE) != O_WRONLY &&
	    (flags & O_ACCMODE) != O_RDWR) || !(flags & O_NONBLOCK) ||
	    fstat(5, &info) != 0 || !S_ISFIFO(info.st_mode) ||
	    !fvwm_fd_set_cloexec(5, true))
		return -1;
	return 5;
}

bool session_leases_prepare_restart(int lease_descriptor, int bus_descriptor)
{
	session_leases_finish_restart(lease_descriptor, bus_descriptor);
	if ((lease_descriptor >= 0 &&
	     (lease_descriptor != 4 ||
	      setenv("FVWM_SESSION_LEASE_FD", "4", 1) != 0 ||
	      !fvwm_fd_set_cloexec(lease_descriptor, false))) ||
	    (bus_descriptor >= 0 &&
	     (bus_descriptor != 5 ||
	      setenv("FVWM_SESSION_BUS_LIFETIME_FD", "5", 1) != 0 ||
	      !fvwm_fd_set_cloexec(bus_descriptor, false))))
	{
		session_leases_finish_restart(lease_descriptor, bus_descriptor);
		return false;
	}
	return true;
}

void session_leases_finish_restart(int lease_descriptor, int bus_descriptor)
{
	int saved_errno = errno;

	unsetenv("FVWM_SESSION_LEASE_FD");
	unsetenv("FVWM_SESSION_BUS_LIFETIME_FD");
	if (lease_descriptor >= 0)
		(void)fvwm_fd_set_cloexec(lease_descriptor, true);
	if (bus_descriptor >= 0)
		(void)fvwm_fd_set_cloexec(bus_descriptor, true);
	errno = saved_errno;
}

void session_ready_publish(int *descriptor, int lease_descriptor,
	int bus_descriptor)
{
	if (*descriptor >= 0)
	{
		char acknowledgment[3] = {'R', 0, 0};
		size_t length = 1;

		if (lease_descriptor >= 0)
			acknowledgment[length++] = 'L';
		if (bus_descriptor >= 0)
			acknowledgment[length++] = 'B';
		/* A full pipe or missing reader cannot stall/terminate the X owner. */
		(void)fvwm_write_without_sigpipe(
			*descriptor, acknowledgment, length);
		close(*descriptor);
		*descriptor = -1;
	}
}
