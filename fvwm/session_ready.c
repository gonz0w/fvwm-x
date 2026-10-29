/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "session_ready.h"
#include "libs/fd_safety.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
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

void session_ready_publish(int *descriptor)
{
	if (*descriptor >= 0)
	{
		/* A full pipe or missing reader cannot stall/terminate the X owner. */
		(void)fvwm_write_without_sigpipe(*descriptor, "R", 1);
		close(*descriptor);
		*descriptor = -1;
	}
}
