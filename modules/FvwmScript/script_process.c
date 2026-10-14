/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "script_process.h"
#include "script_output.h"
#include "libs/fd_safety.h"
#include "libs/safemalloc.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int64_t monotonic_ms(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
		return -1;
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void terminate_command(pid_t child)
{
	/* Keep the direct child unreaped until all group signals are sent,
	 * so its PID cannot be recycled into another command's group ID. */
	kill(-child, SIGTERM);
	kill(child, SIGTERM);
	int64_t end = monotonic_ms();
	if (end >= 0)
	{
		end += 100;
		for (;;)
		{
			int64_t now = monotonic_ms();
			if (now < 0 || now >= end)
				break;
			poll(NULL, 0, (int)(end - now));
		}
	}
	kill(-child, SIGKILL);
	kill(child, SIGKILL);
	while (waitpid(child, NULL, 0) < 0 && errno == EINTR)
	{
	}
}

bool script_process_read(const char *command, int timeout_ms, char **text,
	size_t *length)
{
	int pipes[2], flags, saved_errno;
	char *buffer;
	size_t used = 0;
	pid_t child;
	bool open_output = true;
	int64_t now, deadline;
	sigset_t blocked, previous;
	struct sigaction child_action;

	*text = NULL;
	*length = 0;
	if (command == NULL || timeout_ms < 1 ||
	    timeout_ms > SCRIPT_PROCESS_MAX_TIMEOUT_MS ||
	    strlen(command) > SCRIPT_OUTPUT_COMMAND_MAX_BYTES)
	{
		errno = EINVAL;
		return false;
	}
	/* This synchronous module helper must own its child until cleanup.
	 * Competing/automatic reaping would permit process-group ID reuse. */
	if (sigaction(SIGCHLD, NULL, &child_action) < 0)
		return false;
	if (child_action.sa_handler != SIG_DFL ||
	    (child_action.sa_flags & SA_NOCLDWAIT) != 0)
	{
		errno = EINVAL;
		return false;
	}
	now = monotonic_ms();
	if (now < 0)
		return false;
	deadline = now + timeout_ms;
	if (!fvwm_pipe_cloexec(pipes))
		return false;
	flags = fcntl(pipes[0], F_GETFL);
	if (flags < 0 || fcntl(pipes[0], F_SETFL, flags | O_NONBLOCK) < 0)
		goto setup_failed;
	buffer = fxmalloc(SCRIPT_OUTPUT_MAX_BYTES + 1);
	/* A timed-out child must never run the module's inherited Quit/X
	 * handlers in the interval between fork and exec. */
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGTERM);
	sigaddset(&blocked, SIGINT);
	sigaddset(&blocked, SIGHUP);
	sigaddset(&blocked, SIGPIPE);
	if (sigprocmask(SIG_BLOCK, &blocked, &previous) < 0)
	{
		free(buffer);
		goto setup_failed;
	}
	child = fork();
	if (child < 0)
	{
		saved_errno = errno;
		sigprocmask(SIG_SETMASK, &previous, NULL);
		errno = saved_errno;
		free(buffer);
		goto setup_failed;
	}
	if (child == 0)
	{
		struct sigaction action = {0};
		action.sa_handler = SIG_DFL;
		sigemptyset(&action.sa_mask);
		if (sigaction(SIGTERM, &action, NULL) < 0 ||
		    sigaction(SIGINT, &action, NULL) < 0 ||
		    sigaction(SIGHUP, &action, NULL) < 0 ||
		    sigaction(SIGPIPE, &action, NULL) < 0)
			_exit(127);
		if (setpgid(0, 0) < 0 || dup2(pipes[1], STDOUT_FILENO) < 0 ||
		    !fvwm_fd_set_cloexec(STDOUT_FILENO, false))
			_exit(127);
		if (pipes[0] != STDOUT_FILENO)
			close(pipes[0]);
		if (pipes[1] != STDOUT_FILENO)
			close(pipes[1]);
		if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0)
			_exit(127);
		execl("/bin/sh", "sh", "-c", command, (char *)NULL);
		_exit(127);
	}
	close(pipes[1]);
	pipes[1] = -1;
	if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0)
		goto command_failed;
	/* Establish the group even when the child has not yet been scheduled.
	 * EACCES means it already exec'd after its own successful setpgid. */
	if (setpgid(child, child) < 0 && errno != EACCES && errno != ESRCH)
		goto command_failed;
	for (;;)
	{
		now = monotonic_ms();
		if (now < 0)
			goto command_failed;
		if (now >= deadline)
		{
			errno = ETIMEDOUT;
			goto command_failed;
		}
		if (!open_output)
		{
			pid_t result = waitpid(child, NULL, WNOHANG);
			if (result == child)
				break;
			if (result < 0 && errno != EINTR)
			{
				/* A competing reaper relinquishes the PID;
				 * do not signal it. */
				if (errno == ECHILD)
				{
					free(buffer);
					return false;
				}
				goto command_failed;
			}
		}
		struct pollfd input = {pipes[0], POLLIN, 0};
		int remaining = (int)(deadline - now);
		int wait = open_output ? remaining :
			(remaining > 10 ? 10 : remaining);
		int ready = poll(open_output ? &input : NULL,
			open_output ? 1 : 0, wait);
		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			goto command_failed;
		}
		if (ready == 0 || !open_output)
			continue;
		if (input.revents & POLLNVAL)
		{
			errno = EBADF;
			goto command_failed;
		}
		ssize_t count = read(pipes[0], buffer + used,
			SCRIPT_OUTPUT_MAX_BYTES - used);
		if (count < 0)
		{
			if (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				continue;
			goto command_failed;
		}
		used += count;
		if (count == 0 || used == SCRIPT_OUTPUT_MAX_BYTES)
		{
			close(pipes[0]);
			pipes[0] = -1;
			open_output = false;
		}
	}
	buffer[used] = '\0';
	if (memchr(buffer, '\0', used) != NULL)
	{
		free(buffer);
		errno = EINVAL;
		return false;
	}
	*text = buffer;
	*length = used;
	return true;

command_failed:
	saved_errno = errno;
	if (pipes[0] >= 0)
		close(pipes[0]);
	if (pipes[1] >= 0)
		close(pipes[1]);
	terminate_command(child);
	free(buffer);
	errno = saved_errno;
	return false;
setup_failed:
	saved_errno = errno;
	close(pipes[0]);
	close(pipes[1]);
	errno = saved_errno;
	return false;
}
