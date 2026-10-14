/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/fd_safety.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void
test_pipe_flags_and_exec(const char *executable)
{
	int descriptors[2], status;
	char first[32], second[32];

	assert(fvwm_pipe_cloexec(descriptors));
	assert(fcntl(descriptors[0], F_GETFD) & FD_CLOEXEC);
	assert(fcntl(descriptors[1], F_GETFD) & FD_CLOEXEC);
	assert(fvwm_fd_set_cloexec(descriptors[1], false));
	snprintf(first, sizeof(first), "%d", descriptors[0]);
	snprintf(second, sizeof(second), "%d", descriptors[1]);
	pid_t child = fork();
	assert(child >= 0);
	if (child == 0)
	{
		execl(executable, executable, first, second, (char *)NULL);
		_exit(127);
	}
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	close(descriptors[0]);
	close(descriptors[1]);
	assert(!fvwm_fd_set_cloexec(-1, true));
}

static void
test_lock_file_rejects_unsafe_objects(void)
{
	char directory[] = "/tmp/fvwm-fd-test-XXXXXX";
	char path[256], alias[256];

	assert(mkdtemp(directory) != NULL);
	snprintf(path, sizeof(path), "%s/lock", directory);
	snprintf(alias, sizeof(alias), "%s/alias", directory);
	int descriptor = fvwm_open_private_lock(path);
	assert(descriptor >= 0);
	assert(fcntl(descriptor, F_GETFD) & FD_CLOEXEC);
	close(descriptor);
	assert(symlink(path, alias) == 0);
	assert(fvwm_open_private_lock(alias) == -1);
	assert(unlink(alias) == 0);
	assert(link(path, alias) == 0);
	assert(fvwm_open_private_lock(path) == -1);
	assert(unlink(alias) == 0);
	assert(chmod(path, 0644) == 0);
	assert(fvwm_open_private_lock(path) == -1);
	assert(unlink(path) == 0);
	assert(mkfifo(path, 0600) == 0);
	/* O_NONBLOCK prevents a hostile FIFO from blocking before fstat. */
	assert(fvwm_open_private_lock(path) == -1);
	assert(unlink(path) == 0);
	assert(rmdir(directory) == 0);
}

int
main(int argc, char **argv)
{
	if (argc == 3)
	{
		assert(fcntl(atoi(argv[1]), F_GETFD) == -1 && errno == EBADF);
		assert(fcntl(atoi(argv[2]), F_GETFD) >= 0);
		return 0;
	}
	test_pipe_flags_and_exec(argv[0]);
	test_lock_file_rejects_unsafe_objects();
	return 0;
}
