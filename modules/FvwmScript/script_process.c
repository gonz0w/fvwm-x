/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "script_process.h"
#include "script_output.h"
#include "libs/fd_safety.h"
#include "libs/fvwmsignal.h"
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

/* Only async-signal-safe operations are permitted after fork. The close-on-exec
 * writer closes on successful exec; an errno payload identifies setup failure. */
static void child_failed(int status_fd)
{
	int error = errno;
	const char *bytes = (const char *)&error;
	size_t remaining = sizeof(error);

	while (remaining > 0)
	{
		ssize_t count = write(status_fd, bytes, remaining);
		if (count < 0 && errno == EINTR)
			continue;
		if (count <= 0)
			break;
		bytes += count;
		remaining -= count;
	}
	_exit(127);
}

bool script_process_read(const char *command, int timeout_ms, char **text,
	size_t *length)
{
	int pipes[2] = {-1, -1}, status_pipe[2] = {-1, -1};
	int flags, saved_errno, child_errno = 0;
	char *buffer = NULL;
	size_t used = 0, status_used = 0;
	pid_t child = -1;
	int64_t now, deadline;
	sigset_t blocked, previous;
	struct sigaction child_action;
	bool mask_blocked = false;

	*text = NULL;
	*length = 0;
	if (isTerminated)
	{
		errno = ECANCELED;
		return false;
	}
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
	if (!fvwm_pipe_cloexec(pipes) || !fvwm_pipe_cloexec(status_pipe))
		goto failed;
	flags = fcntl(pipes[0], F_GETFL);
	if (flags < 0 || fcntl(pipes[0], F_SETFL, flags | O_NONBLOCK) < 0)
		goto failed;
	buffer = fxmalloc(SCRIPT_OUTPUT_MAX_BYTES + 1);
	/* A failed child must never run the module's inherited Quit/X handlers
	 * in the interval between fork and exec. */
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGTERM);
	sigaddset(&blocked, SIGINT);
	sigaddset(&blocked, SIGHUP);
	sigaddset(&blocked, SIGPIPE);
	if (sigprocmask(SIG_BLOCK, &blocked, &previous) < 0)
		goto failed;
	mask_blocked = true;
	if (isTerminated)
	{
		errno = ECANCELED;
		goto failed;
	}
	child = fork();
	if (child < 0)
		goto failed;
	if (child == 0)
	{
		struct sigaction action = {0};
		action.sa_handler = SIG_DFL;
		sigemptyset(&action.sa_mask);
		close(status_pipe[0]);
		if (sigaction(SIGTERM, &action, NULL) < 0 ||
		    sigaction(SIGINT, &action, NULL) < 0 ||
		    sigaction(SIGHUP, &action, NULL) < 0 ||
		    sigaction(SIGPIPE, &action, NULL) < 0)
			child_failed(status_pipe[1]);
		if (setpgid(0, 0) < 0 || dup2(pipes[1], STDOUT_FILENO) < 0 ||
		    !fvwm_fd_set_cloexec(STDOUT_FILENO, false))
			child_failed(status_pipe[1]);
		if (pipes[0] != STDOUT_FILENO)
			close(pipes[0]);
		if (pipes[1] != STDOUT_FILENO)
			close(pipes[1]);
		if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0)
			child_failed(status_pipe[1]);
		execl("/bin/sh", "sh", "-c", command, (char *)NULL);
		child_failed(status_pipe[1]);
	}
	close(pipes[1]);
	pipes[1] = -1;
	close(status_pipe[1]);
	status_pipe[1] = -1;
	if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0)
		goto failed;
	mask_blocked = false;
	/* Establish the group even when the child has not yet been scheduled.
	 * EACCES means it already exec'd after its own successful setpgid. */
	if (setpgid(child, child) < 0 && errno != EACCES && errno != ESRCH)
		goto failed;
	for (;;)
	{
		/* Check before waitpid: cancellation must retain the owned PID until
		 * all group signals have been sent. Never signal a reaped child. */
		if (isTerminated)
		{
			errno = ECANCELED;
			goto failed;
		}
		now = monotonic_ms();
		if (now < 0)
			goto failed;
		if (now >= deadline)
		{
			errno = ETIMEDOUT;
			goto failed;
		}
		if (pipes[0] < 0 && status_pipe[0] < 0)
		{
			pid_t result = waitpid(child, NULL, WNOHANG);
			if (result == child)
			{
				child = -1;
				break;
			}
			if (result < 0 && errno != EINTR)
			{
				/* A competing reaper relinquishes the PID. */
				if (errno == ECHILD)
					child = -1;
				goto failed;
			}
		}
		struct pollfd inputs[2] = {
			{pipes[0], POLLIN, 0}, {status_pipe[0], POLLIN, 0}};
		int remaining = (int)(deadline - now);
		int wait = remaining > 100 ? 100 : remaining;
		if (pipes[0] < 0 && status_pipe[0] < 0 && wait > 10)
			wait = 10;
		/* Bound the check/begin-wait race without a signal-handler jump:
		 * every wait rechecks termination within 100 ms. Cleanup also
		 * uses raw poll so an already-set flag cannot spin. */
		int ready = poll(inputs, 2, wait);
		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			goto failed;
		}
		if (ready == 0)
			continue;
		if ((inputs[0].revents | inputs[1].revents) & POLLNVAL)
		{
			errno = EBADF;
			goto failed;
		}
		if (inputs[1].revents)
		{
			ssize_t count = read(status_pipe[0],
				(char *)&child_errno + status_used,
				sizeof(child_errno) - status_used);
			if (count < 0)
			{
				if (errno == EINTR)
					continue;
				goto failed;
			}
			status_used += count;
			if (status_used == sizeof(child_errno))
			{
				errno = child_errno;
				goto failed;
			}
			if (count == 0)
			{
				if (status_used != 0)
				{
					errno = EIO;
					goto failed;
				}
				close(status_pipe[0]);
				status_pipe[0] = -1;
			}
		}
		if (!inputs[0].revents)
			continue;
		ssize_t count = read(pipes[0], buffer + used,
			SCRIPT_OUTPUT_MAX_BYTES - used);
		if (count < 0)
		{
			if (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				continue;
			goto failed;
		}
		used += count;
		if (count == 0 || used == SCRIPT_OUTPUT_MAX_BYTES)
		{
			close(pipes[0]);
			pipes[0] = -1;
		}
	}
	if (isTerminated)
	{
		errno = ECANCELED;
		goto failed;
	}
	buffer[used] = '\0';
	if (memchr(buffer, '\0', used) != NULL)
	{
		errno = EINVAL;
		goto failed;
	}
	*text = buffer;
	*length = used;
	return true;

failed:
	saved_errno = errno;
	/* Retry a failed parent restoration once during common cleanup. A
	 * persistent OS refusal cannot be repaired here; retain the first error. */
	if (mask_blocked)
	{
		for (int attempt = 0; attempt < 2; attempt++)
		{
			if (sigprocmask(SIG_SETMASK, &previous, NULL) == 0)
				break;
		}
	}
	for (int i = 0; i < 2; i++)
	{
		if (pipes[i] >= 0)
			close(pipes[i]);
		if (status_pipe[i] >= 0)
			close(status_pipe[i]);
	}
	if (child > 0)
		terminate_command(child);
	free(buffer);
	errno = saved_errno;
	return false;
}
