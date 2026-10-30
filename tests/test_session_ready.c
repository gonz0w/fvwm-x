/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "fvwm/session_ready.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int make_ready_pipe(void)
{
	int descriptors[2];
	int reservation = open("/dev/null", O_RDONLY);

	assert(reservation == 3);
	assert(pipe(descriptors) == 0);
	assert(dup2(descriptors[1], 3) == 3);
	close(descriptors[1]);
	assert(fcntl(3, F_SETFL, O_NONBLOCK) == 0);
	assert(setenv("FVWM_SESSION_READY_FD", "3", 1) == 0);
	return descriptors[0];
}

static void test_environment_and_refusals(void)
{
	const char *invalid[] = {"", "0", "4", "03", "3junk", "-1"};
	size_t i;
	int reader;
	int regular;
	int flags;

	unsetenv("FVWM_SESSION_READY_FD");
	assert(session_ready_take() == -1);
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		assert(setenv("FVWM_SESSION_READY_FD", invalid[i], 1) == 0);
		assert(session_ready_take() == -1);
		assert(getenv("FVWM_SESSION_READY_FD") == NULL);
	}
	assert(setenv("FVWM_SESSION_READY_FD", "3", 1) == 0);
	assert(session_ready_take() == -1);
	regular = open("/dev/null", O_WRONLY | O_NONBLOCK);
	assert(regular == 3);
	assert(setenv("FVWM_SESSION_READY_FD", "3", 1) == 0);
	assert(session_ready_take() == -1);
	close(regular);
	reader = make_ready_pipe();
	assert(fcntl(3, F_SETFL, 0) == 0);
	assert(session_ready_take() == -1);
	close(3);
	assert(dup2(reader, 3) == 3);
	assert(fcntl(3, F_SETFL, O_NONBLOCK) == 0);
	assert(setenv("FVWM_SESSION_READY_FD", "3", 1) == 0);
	flags = fcntl(3, F_GETFL);
	/* A duplex BSD pipe end is writable even when returned as the reader.
	 * A genuinely read-only endpoint must still be refused. */
	if ((flags & O_ACCMODE) == O_RDONLY)
	{
		assert(session_ready_take() == -1);
	}
	else
	{
		assert((flags & O_ACCMODE) == O_RDWR);
		assert(session_ready_take() == 3);
	}
	close(3);
	close(reader);
}

static void test_one_shot_flags_and_bytes(void)
{
	int reader = make_ready_pipe();
	int descriptor = session_ready_take();
	char bytes[2];

	assert(descriptor == 3);
	assert(getenv("FVWM_SESSION_READY_FD") == NULL);
	assert(fcntl(descriptor, F_GETFL) & O_NONBLOCK);
	assert(fcntl(descriptor, F_GETFD) & FD_CLOEXEC);
	session_ready_publish(&descriptor, -1, -1);
	assert(descriptor == -1);
	assert(fcntl(3, F_GETFD) == -1 && errno == EBADF);
	assert(read(reader, bytes, sizeof(bytes)) == 1 && bytes[0] == 'R');
	session_ready_publish(&descriptor, -1, -1);
	assert(read(reader, bytes, sizeof(bytes)) == 0);
	close(reader);
}

static void test_full_pipe_and_missing_reader(int lease_descriptor)
{
	int reader = make_ready_pipe();
	int descriptor = session_ready_take();
	char bytes[4096];
	struct sigaction action;
	struct sigaction after;

	memset(bytes, 'x', sizeof(bytes));
	while (write(descriptor, bytes, sizeof(bytes)) > 0)
	{
	}
	assert(errno == EAGAIN || errno == EWOULDBLOCK);
	session_ready_publish(&descriptor, lease_descriptor, -1);
	assert(descriptor == -1);
	close(reader);
	reader = make_ready_pipe();
	descriptor = session_ready_take();
	close(reader);
	memset(&action, 0, sizeof(action));
	sigemptyset(&action.sa_mask);
	action.sa_handler = SIG_DFL;
	assert(sigaction(SIGPIPE, &action, NULL) == 0);
	session_ready_publish(&descriptor, lease_descriptor, -1);
	assert(descriptor == -1);
	assert(sigaction(SIGPIPE, NULL, &after) == 0);
	assert(after.sa_handler == SIG_DFL);
}

static void expect_child(pid_t child)
{
	int status;

	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void request_lease(void)
{
	assert(setenv("FVWM_SESSION_LEASE_FD", "4", 1) == 0);
}

static void make_lease(char *path)
{
	int reservation = open("/dev/null", O_RDONLY);

	assert(reservation == 3);
	assert(mkstemp(path) == 4);
	close(reservation);
	assert(fchmod(4, 0600) == 0);
	assert(fcntl(4, F_SETFL, O_NONBLOCK) == 0);
	assert(flock(4, LOCK_EX | LOCK_NB) == 0);
	request_lease();
}

static void test_lease_refusals(void)
{
	const char *invalid[] = {"", "0", "3", "04", "4junk", "-1"};
	char path[] = "/tmp/fvwm-session-lease-XXXXXX";
	char linked[sizeof(path) + 5];
	size_t i;
	int descriptor;
	int reader;
	int flags;
	int modes[] = {O_RDONLY, O_WRONLY};

	unsetenv("FVWM_SESSION_LEASE_FD");
	assert(session_lease_take() == -1);
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		assert(setenv("FVWM_SESSION_LEASE_FD", invalid[i], 1) == 0);
		assert(session_lease_take() == -1);
		assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
	}
	request_lease();
	assert(session_lease_take() == -1);
	reader = make_ready_pipe();
	/* Its FIFO reader occupies FD4; neither pipe direction is a lease. */
	assert(reader == 4);
	request_lease();
	assert(session_lease_take() == -1);
	close(3);
	close(reader);
	unsetenv("FVWM_SESSION_READY_FD");
	make_lease(path);
	assert(fchmod(4, 0640) == 0);
	assert(session_lease_take() == -1);
	assert(fchmod(4, 0600) == 0);
	strcpy(linked, path);
	strcat(linked, ".link");
	assert(link(path, linked) == 0);
	request_lease();
	assert(session_lease_take() == -1);
	assert(unlink(linked) == 0);
	assert(fcntl(4, F_SETFL, 0) == 0);
	request_lease();
	assert(session_lease_take() == -1);
	assert(fcntl(4, F_SETFL, O_NONBLOCK) == 0);
	/* Exercise foreign ownership when the unit executor can create it. */
	if (geteuid() == 0)
	{
		assert(fchown(4, getuid() + 1, (gid_t)-1) == 0);
		request_lease();
		assert(session_lease_take() == -1);
		assert(fchown(4, getuid(), (gid_t)-1) == 0);
	}
	close(4);
	for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
	{
		descriptor = open(path, modes[i] | O_NONBLOCK);
		assert(descriptor == 3);
		assert(dup2(descriptor, 4) == 4);
		close(descriptor);
		request_lease();
		assert(session_lease_take() == -1);
		close(4);
	}
	descriptor = open(path, O_RDWR | O_NONBLOCK);
	assert(descriptor == 3);
	assert(dup2(descriptor, 4) == 4);
	close(descriptor);
	/* An independent open-file description owns the competing flock. */
	descriptor = open(path, O_RDWR | O_NONBLOCK);
	assert(descriptor == 3);
	assert(flock(descriptor, LOCK_EX | LOCK_NB) == 0);
	request_lease();
	assert(session_lease_take() == -1);
	assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
	close(descriptor);
	request_lease();
	assert(session_lease_take() == 4);
	flags = fcntl(4, F_GETFD);
	assert(flags >= 0 && (flags & FD_CLOEXEC));
	close(4);
	assert(unlink(path) == 0);
}

static void test_lease_readiness(void)
{
	char path[] = "/tmp/fvwm-session-lease-XXXXXX";
	char bytes[3];
	int reader;
	int ready;
	int lease;
	int contender;

	make_lease(path);
	lease = session_lease_take();
	assert(lease == 4);
	assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
	assert(fcntl(lease, F_GETFL) & O_NONBLOCK);
	assert(fcntl(lease, F_GETFD) & FD_CLOEXEC);
	reader = make_ready_pipe();
	ready = session_ready_take();
	session_ready_publish(&ready, lease, -1);
	assert(ready == -1);
	assert(read(reader, bytes, sizeof(bytes)) == 2);
	assert(memcmp(bytes, "RL", 2) == 0);
	assert(read(reader, bytes, sizeof(bytes)) == 0);
	close(reader);
	test_full_pipe_and_missing_reader(lease);
	contender = open(path, O_RDWR | O_NONBLOCK);
	assert(contender == 3);
	assert(flock(contender, LOCK_EX | LOCK_NB) == -1);
	assert(errno == EAGAIN || errno == EWOULDBLOCK);
	close(lease);
	assert(flock(contender, LOCK_EX | LOCK_NB) == 0);
	close(contender);
	assert(unlink(path) == 0);
}

static void test_inherited_lease(void)
{
	char path[] = "/tmp/fvwm-session-lease-XXXXXX";
	int control[2];
	int acknowledgment[2];
	int contender;
	char byte;
	pid_t child;

	make_lease(path);
	assert(pipe(control) == 0);
	assert(pipe(acknowledgment) == 0);
	child = fork();
	assert(child >= 0);
	if (child == 0)
	{
		close(control[1]);
		close(acknowledgment[0]);
		assert(session_lease_take() == 4);
		assert(write(acknowledgment[1], "L", 1) == 1);
		assert(read(control[0], &byte, 1) == 1);
		close(4);
		_exit(0);
	}
	close(control[0]);
	close(acknowledgment[1]);
	assert(read(acknowledgment[0], &byte, 1) == 1 && byte == 'L');
	close(acknowledgment[0]);
	close(4);
	unsetenv("FVWM_SESSION_LEASE_FD");
	contender = open(path, O_RDWR | O_NONBLOCK);
	assert(contender >= 0);
	/* The parent's close must leave the child's inherited flock held. */
	assert(flock(contender, LOCK_EX | LOCK_NB) == -1);
	assert(errno == EAGAIN || errno == EWOULDBLOCK);
	assert(write(control[1], "C", 1) == 1);
	close(control[1]);
	expect_child(child);
	assert(flock(contender, LOCK_EX | LOCK_NB) == 0);
	close(contender);
	assert(unlink(path) == 0);
}

static void test_restart(const char *binary)
{
	char path[] = "/tmp/fvwm-session-lease-XXXXXX";
	char *missing[] = {"/fvwm-session-test-missing-executable", NULL};
	pid_t child;
	int native;

	make_lease(path);
	assert(session_lease_take() == 4);
	assert(session_leases_prepare_restart(4, -1));
	assert(strcmp(getenv("FVWM_SESSION_LEASE_FD"), "4") == 0);
	assert(!(fcntl(4, F_GETFD) & FD_CLOEXEC));
	assert(execv(missing[0], missing) == -1);
	session_leases_finish_restart(4, -1);
	assert(errno == ENOENT);
	assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
	assert(fcntl(4, F_GETFD) & FD_CLOEXEC);
	for (native = 0; native <= 1; native++)
	{
		child = fork();
		assert(child >= 0);
		if (child == 0)
		{
			if (native)
			{
				assert(session_leases_prepare_restart(4, -1));
			}
			execl(binary, binary, native ? "--lease" : "--no-lease",
			    (char *)NULL);
			_exit(127);
		}
		expect_child(child);
		assert(fcntl(4, F_GETFD) & FD_CLOEXEC);
		assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
	}
	close(4);
	assert(!session_leases_prepare_restart(4, -1));
	assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
	assert(session_leases_prepare_restart(-1, -1));
	session_leases_finish_restart(-1, -1);
	assert(unlink(path) == 0);
}

static int make_bus_lifetime(void)
{
	int descriptors[2];
	int reader;

	assert(pipe(descriptors) == 0);
	reader = fcntl(descriptors[0], F_DUPFD, 10);
	assert(reader >= 10);
	assert(dup2(descriptors[1], 5) == 5);
	close(descriptors[0]);
	if (descriptors[1] != 5)
		close(descriptors[1]);
	assert(fcntl(5, F_SETFL, O_NONBLOCK) == 0);
	assert(setenv("FVWM_SESSION_BUS_LIFETIME_FD", "5", 1) == 0);
	return reader;
}

static void test_bus_lifetime(const char *binary)
{
	const char *invalid[] = {"", "0", "4", "05", "5junk", "-1"};
	char path[] = "/tmp/fvwm-session-lease-XXXXXX";
	char bytes[4];
	int reader, ready_reader, ready, native;
	size_t i;
	pid_t child;

	unsetenv("FVWM_SESSION_BUS_LIFETIME_FD");
	assert(session_bus_lifetime_take() == -1);
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		assert(setenv("FVWM_SESSION_BUS_LIFETIME_FD", invalid[i], 1) == 0);
		assert(session_bus_lifetime_take() == -1);
		assert(getenv("FVWM_SESSION_BUS_LIFETIME_FD") == NULL);
	}
	reader = make_bus_lifetime();
	assert(fcntl(5, F_SETFL, 0) == 0);
	assert(session_bus_lifetime_take() == -1);
	assert(setenv("FVWM_SESSION_BUS_LIFETIME_FD", "5", 1) == 0);
	assert(fcntl(5, F_SETFL, O_NONBLOCK) == 0);
	assert(session_bus_lifetime_take() == 5);
	assert(getenv("FVWM_SESSION_BUS_LIFETIME_FD") == NULL);
	assert(fcntl(5, F_GETFD) & FD_CLOEXEC);
	ready_reader = make_ready_pipe();
	ready = session_ready_take();
	session_ready_publish(&ready, -1, 5);
	assert(read(ready_reader, bytes, sizeof(bytes)) == 2);
	assert(memcmp(bytes, "RB", 2) == 0);
	close(ready_reader);
	make_lease(path);
	assert(session_lease_take() == 4);
	ready_reader = make_ready_pipe();
	ready = session_ready_take();
	session_ready_publish(&ready, 4, 5);
	assert(read(ready_reader, bytes, sizeof(bytes)) == 3);
	assert(memcmp(bytes, "RLB", 3) == 0);
	close(ready_reader);
	/* A failed second preparation restores FD4 and both private markers. */
	assert(!session_leases_prepare_restart(4, 99));
	assert(fcntl(4, F_GETFD) & FD_CLOEXEC);
	assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
	assert(getenv("FVWM_SESSION_BUS_LIFETIME_FD") == NULL);
	for (native = 0; native <= 1; native++)
	{
		child = fork();
		assert(child >= 0);
		if (child == 0)
		{
			close(reader);
			if (native)
				assert(session_leases_prepare_restart(4, 5));
			execl(binary, binary, native ? "--both" : "--neither",
				(char *)NULL);
			_exit(127);
		}
		expect_child(child);
		assert(fcntl(4, F_GETFD) & FD_CLOEXEC);
		assert(fcntl(5, F_GETFD) & FD_CLOEXEC);
	}
	assert(session_leases_prepare_restart(4, 5));
	session_leases_finish_restart(4, 5);
	assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
	assert(getenv("FVWM_SESSION_BUS_LIFETIME_FD") == NULL);
	assert(fcntl(4, F_GETFD) & FD_CLOEXEC);
	assert(fcntl(5, F_GETFD) & FD_CLOEXEC);
	close(5);
	assert(!session_leases_prepare_restart(4, 5));
	assert(fcntl(4, F_GETFD) & FD_CLOEXEC);
	assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
	assert(getenv("FVWM_SESSION_BUS_LIFETIME_FD") == NULL);
	close(4);
	assert(read(reader, bytes, 1) == 0);
	close(reader);
	assert(unlink(path) == 0);
	/* Regular descriptors can never serve as bus lifetime holders. */
	reader = open("/dev/null", O_WRONLY | O_NONBLOCK);
	assert(reader == 3);
	assert(dup2(reader, 5) == 5);
	close(reader);
	assert(setenv("FVWM_SESSION_BUS_LIFETIME_FD", "5", 1) == 0);
	assert(session_bus_lifetime_take() == -1);
	assert(getenv("FVWM_SESSION_BUS_LIFETIME_FD") == NULL);
	close(5);
	{
		int endpoints[2];
		int flags;

		assert(pipe(endpoints) == 0);
		assert(dup2(endpoints[0], 5) == 5);
		close(endpoints[0]);
		assert(fcntl(5, F_SETFL, O_NONBLOCK) == 0);
		assert(setenv("FVWM_SESSION_BUS_LIFETIME_FD", "5", 1) == 0);
		flags = fcntl(5, F_GETFL);
		if ((flags & O_ACCMODE) == O_RDONLY)
			assert(session_bus_lifetime_take() == -1);
		else
		{
			assert((flags & O_ACCMODE) == O_RDWR);
			assert(session_bus_lifetime_take() == 5);
		}
		close(5);
		close(endpoints[1]);
	}
}

int main(int argc, char **argv)
{
	if (argc == 2)
	{
		if (strcmp(argv[1], "--both") == 0)
		{
			assert(session_lease_take() == 4);
			assert(session_bus_lifetime_take() == 5);
			assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
			assert(getenv("FVWM_SESSION_BUS_LIFETIME_FD") == NULL);
			assert(fcntl(4, F_GETFD) & FD_CLOEXEC);
			assert(fcntl(5, F_GETFD) & FD_CLOEXEC);
			return 0;
		}
		if (strcmp(argv[1], "--neither") == 0)
		{
			assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
			assert(getenv("FVWM_SESSION_BUS_LIFETIME_FD") == NULL);
			assert(fcntl(4, F_GETFD) == -1 && errno == EBADF);
			assert(fcntl(5, F_GETFD) == -1 && errno == EBADF);
			return 0;
		}
		if (strcmp(argv[1], "--lease") == 0)
		{
			assert(session_lease_take() == 4);
			assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
			assert(fcntl(4, F_GETFD) & FD_CLOEXEC);
		}
		else
		{
			assert(strcmp(argv[1], "--no-lease") == 0);
			assert(getenv("FVWM_SESSION_LEASE_FD") == NULL);
			assert(fcntl(4, F_GETFD) == -1 && errno == EBADF);
		}
		return 0;
	}
	test_environment_and_refusals();
	test_one_shot_flags_and_bytes();
	test_full_pipe_and_missing_reader(-1);
	test_lease_refusals();
	test_lease_readiness();
	test_inherited_lease();
	test_restart(argv[0]);
	test_bus_lifetime(argv[0]);
	return 0;
}
