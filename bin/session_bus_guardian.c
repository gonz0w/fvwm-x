/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "session_bus_guardian.h"
#include "libs/fd_safety.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ADDRESS_MAX 4096
#define OUTPUT_MAX (ADDRESS_MAX + 32)
#define START_MS 5000
#define TERM_MS 2000
#define REPORT_MS 1000

/* One invocation owns this wake endpoint. The handler never touches a child
 * handle or performs a reap; a full pipe already supplies the required wake. */
static volatile sig_atomic_t guardian_wake_fd = -1;

static void guardian_wake(int number)
{
	int saved_errno = errno;
	char byte = 'C';

	(void)number;
	if (guardian_wake_fd >= 0)
		(void)write(guardian_wake_fd, &byte, 1);
	errno = saved_errno;
}

static int64_t guardian_now(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return -1;
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static bool guardian_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL);

	return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

static bool guardian_input(const char *path)
{
	struct stat info;
	const char *base;
	int flags = fcntl(3, F_GETFL);

	if (!path || path[0] != '/' || strnlen(path, 4097) > 4096 ||
	    !(base = strrchr(path, '/')) || strcmp(base + 1, "dbus-daemon") ||
	    strstr(path, "//") || strstr(path, "/./") ||
	    strstr(path, "/../") ||
	    flags < 0 || (flags & O_ACCMODE) == O_WRONLY ||
	    fstat(3, &info) != 0 || !S_ISFIFO(info.st_mode))
		return false;
	return stat(path, &info) == 0 && S_ISREG(info.st_mode) &&
		access(path, X_OK) == 0 && guardian_nonblock(3) &&
		fvwm_fd_set_cloexec(3, true) && guardian_nonblock(STDOUT_FILENO);
}

static bool guardian_address(const char *text, size_t length)
{
	size_t i;

	if (!length || length > ADDRESS_MAX ||
	    ((length < 11 || strncmp(text, "unix:path=/", 11) != 0) &&
	     (length <= 14 || strncmp(text, "unix:abstract=", 14) != 0)))
		return false;
	for (i = 0; i < length; i++)
		if ((unsigned char)text[i] < 0x21 ||
		    (unsigned char)text[i] > 0x7e || text[i] == ';')
			return false;
	return true;
}

static int guardian_exit_status(int status)
{
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	if (WIFSIGNALED(status))
	{
		int result = 128 + WTERMSIG(status);
		return result > 255 ? 255 : result;
	}
	return 255;
}

int session_bus_guardian_main(int argc, char **argv)
{
	int address_pipe[2] = {-1, -1};
	int wake[2] = {-1, -1};
	struct sigaction action, previous;
	struct pollfd descriptors[4];
	char address[ADDRESS_MAX + 2];
	char output[OUTPUT_MAX];
	size_t address_length = 0, queued = 0, sent = 0;
	size_t address_output_length = 0;
	pid_t child = -1;
	int status = 0, result = 1;
	int phase = 0;
	int64_t now, deadline, output_deadline = -1;
	bool installed = false, got_address = false, terminal = false;
	bool observer = true, lifetime = true, stopping = false;
	bool output_closed = false, poll_failed = false;

	if (argc != 3 || strcmp(argv[1], "bus-guardian") != 0 ||
	    !guardian_input(argv[2]) || (now = guardian_now()) < 0)
		return 2;
	if (!fvwm_pipe_cloexec(address_pipe) || !fvwm_pipe_cloexec(wake) ||
	    !guardian_nonblock(address_pipe[0]) ||
	    !guardian_nonblock(wake[0]) || !guardian_nonblock(wake[1]))
		goto out;
	memset(&action, 0, sizeof(action));
	sigemptyset(&action.sa_mask);
	action.sa_handler = guardian_wake;
	guardian_wake_fd = wake[1];
	if (sigaction(SIGCHLD, &action, &previous) != 0)
		goto out;
	installed = true;
	child = fork();
	if (child < 0)
		goto out;
	if (child == 0)
	{
		long maximum = sysconf(_SC_OPEN_MAX);
		int fd;

		(void)sigaction(SIGCHLD, &previous, NULL);
		if (dup2(address_pipe[1], STDOUT_FILENO) < 0)
			_exit(127);
		/* The daemon never retains a lifetime writer, the guardian's FD3,
		 * or the parent's receipt stream. No caller argv is forwarded. */
		if (maximum < 0 || maximum > INT_MAX)
			maximum = 65536;
		for (fd = 3; fd < maximum; fd++)
			close(fd);
		execl(argv[2], argv[2], "--session", "--nofork",
			"--print-address=1", "--nopidfile", (char *)NULL);
		_exit(127);
	}
	close(address_pipe[1]);
	address_pipe[1] = -1;
	deadline = now + START_MS;
	for (;;)
	{
		pid_t waited;
		bool failure = false;
		int timeout = -1;
		int64_t next = -1;

		/* Exact wait precedes every signal. Invalidate before any subsequent
		 * event can signal a recycled PID. ECHILD is uncertainty, never W. */
		if (child > 0)
		{
			/* Once failed polling has completed bounded reporting, only an
			 * actual reap remains. Blocking here has no active-work deadline
			 * and cannot discard a kernel-delayed child after U. */
			waited = waitpid(child, &status,
				poll_failed && terminal && output_closed ? 0 : WNOHANG);
			if (waited == child)
			{
				child = -1;
				result = 0;
				if (!terminal)
				{
					queued += (size_t)snprintf(output + queued,
						sizeof(output) - queued, "W %d\n",
						guardian_exit_status(status));
					terminal = true;
					output_deadline = guardian_now() + REPORT_MS;
				}
			}
			else if (waited < 0 && errno != EINTR)
			{
				/* An unexpected external reaper has lost our ownership. */
				child = -1;
				if (!terminal)
				{
					memcpy(output + queued, "U\n", 2);
					queued += 2;
					terminal = true;
					output_deadline = guardian_now() + REPORT_MS;
				}
			}
		}
		now = guardian_now();
		if (now < 0)
		{
			/* CLOCK_MONOTONIC worked before launch. A later clock failure
			 * must not discard the actual child; stop once and reap passively. */
			failure = true;
			now = deadline;
		}
		if (observer && sent < queued)
		{
			ssize_t count = fvwm_write_without_sigpipe(STDOUT_FILENO,
				output + sent, queued - sent);
			if (count > 0)
				sent += (size_t)count;
			else if (count < 0 && errno != EINTR && errno != EAGAIN &&
			    errno != EWOULDBLOCK)
				observer = false;
		}
		if (observer && sent < queued && output_deadline >= 0 &&
		    now >= output_deadline)
			observer = false;
		if (observer && terminal && sent == queued)
		{
			observer = false;
		}
		if (!observer && !output_closed)
		{
			close(STDOUT_FILENO);
			output_closed = true;
		}
		if (child < 0 && (!observer || sent == queued))
			break;
		/* The receipt reader belongs to the portable parent. Its loss after
		 * address delivery cannot end a session retained by the core writer. */
		if (!terminal && !stopping &&
		    ((!observer && (!address_output_length ||
		      sent < address_output_length)) || !lifetime || failure ||
		     poll_failed ||
		     (!got_address && now >= deadline)))
		{
			stopping = true;
			phase = 1;
			(void)kill(child, SIGTERM);
			deadline = now + TERM_MS;
		}
		else if (!terminal && stopping && now >= deadline)
		{
			if (phase == 1)
			{
				(void)kill(child, SIGKILL);
				phase = 2;
				deadline = now + REPORT_MS;
			}
			else
			{
				memcpy(output + queued, "U\n", 2);
				queued += 2;
				terminal = true;
				output_deadline = now + REPORT_MS;
				if (poll_failed)
					continue;
			}
		}
		if (!terminal && (stopping || !got_address))
			next = deadline;
		if (observer && sent < queued && output_deadline >= 0 &&
		    (next < 0 || output_deadline < next))
			next = output_deadline;
		if (next >= 0)
			timeout = next <= now ? 0 : (int)(next - now);
		if (poll_failed)
		{
			/* Do not retry a failed event facility. During bounded cleanup,
			 * sleep only to the next TERM/KILL/report deadline; SIGCHLD
			 * interrupts the sleep. A missed pre-sleep wake merely delays
			 * observation to that finite deadline. After reporting closes,
			 * the next exact wait above blocks without any periodic wake. */
			if (timeout > 0)
			{
				struct timespec delay = {
					timeout / 1000, (timeout % 1000) * 1000000L};

				(void)nanosleep(&delay, NULL);
			}
			continue;
		}
		descriptors[0] = (struct pollfd){wake[0], POLLIN, 0};
		descriptors[1] = (struct pollfd){
			!terminal ? address_pipe[0] : -1, POLLIN, 0};
		descriptors[2] = (struct pollfd){
			lifetime && !terminal && !stopping ? 3 : -1, POLLIN, 0};
		descriptors[3] = (struct pollfd){
			observer ? STDOUT_FILENO : -1,
			sent < queued ? POLLOUT : 0, 0};
		if (poll(descriptors, 4, timeout) < 0)
		{
			if (errno != EINTR)
				poll_failed = true;
			continue;
		}
		if (descriptors[3].revents & (POLLERR | POLLHUP | POLLNVAL))
			observer = false;
		if (descriptors[0].revents)
		{
			char bytes[128];
			/* One bounded drain is enough: exact wait runs each turn. */
			(void)read(wake[0], bytes, sizeof(bytes));
		}
		if (descriptors[2].revents)
		{
			char bytes[128];
			ssize_t count = read(3, bytes, sizeof(bytes));
			/* Lifetime is an EOF-only channel; any payload is malformed. */
			if (count >= 0 || (errno != EAGAIN && errno != EINTR))
				lifetime = false;
		}
		if (descriptors[1].revents)
		{
			ssize_t count = read(address_pipe[0],
				address + address_length,
				sizeof(address) - address_length);
			if (count > 0)
			{
				size_t old = address_length;
				size_t i;
				address_length += (size_t)count;
				for (i = old; i < address_length; i++)
					if (address[i] == '\n')
						break;
				if (got_address || address_length > ADDRESS_MAX + 1 ||
				    (i < address_length && (i + 1 != address_length ||
				     !guardian_address(address, i))))
					lifetime = false;
				else if (i < address_length)
				{
					memcpy(output + queued, address, address_length);
					queued += address_length;
					address_output_length = queued;
					got_address = true;
					output_deadline = guardian_now() + REPORT_MS;
				}
			}
			else if (count == 0 || (errno != EINTR && errno != EAGAIN &&
			    errno != EWOULDBLOCK))
			{
				close(address_pipe[0]);
				address_pipe[0] = -1;
				if (!got_address)
					lifetime = false;
			}
		}
	}
 out:
	/* Every post-fork path goes through the owner loop, including output loss.
	 * In the U state it sleeps on SIGCHLD until the actual child is reaped. */
	guardian_wake_fd = -1;
	if (installed)
		(void)sigaction(SIGCHLD, &previous, NULL);
	if (address_pipe[0] >= 0)
		close(address_pipe[0]);
	if (address_pipe[1] >= 0)
		close(address_pipe[1]);
	if (wake[0] >= 0)
		close(wake[0]);
	if (wake[1] >= 0)
		close(wake[1]);
	return result;
}
