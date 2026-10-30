/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "bin/session_bus_guardian.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct owned_guardian {
	pid_t pid;
	int reader;
	int writer;
};

static const char normal_address[] = "unix:path=/tmp/fvwm-owned-fake\n";

/* The only daemon fixture is this unit executable under an owned private
 * dbus-daemon symlink. Its argv remains the production fixed argv. */
static int fake_daemon(int argc, char **argv)
{
	const char *mode = getenv("FVWM_TEST_GUARDIAN_MODE");
	int fd;

	assert(argc == 5);
	assert(strcmp(argv[1], "--session") == 0);
	assert(strcmp(argv[2], "--nofork") == 0);
	assert(strcmp(argv[3], "--print-address=1") == 0);
	assert(strcmp(argv[4], "--nopidfile") == 0);
	for (fd = 3; fd < 1024; fd++)
		assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
	assert(mode != NULL);
	if (strcmp(mode, "early") == 0)
		return 23;
	if (strcmp(mode, "refuse") == 0)
		assert(signal(SIGTERM, SIG_IGN) != SIG_ERR);
	if (strcmp(mode, "stall") != 0)
	{
		if (strcmp(mode, "malformed") == 0)
			assert(write(1, "not-an-address\n", 15) == 15);
		else if (strcmp(mode, "trailing") == 0)
			assert(write(1, "unix:path=/tmp/fake\nextra\n", 26) == 26);
		else if (strcmp(mode, "maximum") == 0)
		{
			char address[4097];
			memset(address, 'a', sizeof(address));
			memcpy(address, "unix:path=/", 11);
			address[4096] = '\n';
			assert(write(1, address, sizeof(address)) == sizeof(address));
		}
		else
			assert(write(1, normal_address, sizeof(normal_address) - 1) ==
				sizeof(normal_address) - 1);
	}
	for (;;)
		pause();
}

static struct owned_guardian start_guardian(const char *path, const char *mode)
{
	int lifetime[2], output[2];
	struct owned_guardian owned;

	assert(pipe(lifetime) == 0);
	assert(pipe(output) == 0);
	assert(setenv("FVWM_TEST_GUARDIAN_MODE", mode, 1) == 0);
	owned.pid = fork();
	assert(owned.pid >= 0);
	if (owned.pid == 0)
	{
		char *arguments[] = {"guardian-unit", "bus-guardian",
			(char *)path, NULL};
		int reader = fcntl(lifetime[0], F_DUPFD, 10);
		int writer = fcntl(output[1], F_DUPFD, 10);
		int fd;

		assert(reader >= 10 && writer >= 10);
		close(lifetime[0]);
		close(lifetime[1]);
		close(output[0]);
		close(output[1]);
		assert(dup2(reader, 3) == 3);
		assert(dup2(writer, 1) == 1);
		close(reader);
		close(writer);
		/* Sentinel descriptors must not survive into the daemon. */
		fd = open("/dev/null", O_RDWR);
		assert(fd >= 0);
		assert(dup2(fd, 5) == 5);
		if (fd != 5)
			close(fd);
		_exit(session_bus_guardian_main(3, arguments));
	}
	close(lifetime[0]);
	close(output[1]);
	owned.writer = lifetime[1];
	owned.reader = output[0];
	return owned;
}

static void read_line(int fd, char *line, size_t capacity, int timeout)
{
	size_t length = 0;

	while (length + 1 < capacity)
	{
		struct pollfd descriptor = {fd, POLLIN, 0};
		ssize_t count;

		assert(poll(&descriptor, 1, timeout) == 1);
		count = read(fd, line + length, 1);
		assert(count == 1);
		if (line[length++] == '\n')
		{
			line[length] = '\0';
			return;
		}
	}
	assert(!"oversize guardian record");
}

static void expect_wait(struct owned_guardian *owned, int status)
{
	char line[32], expected[32], byte;
	int child_status;

	read_line(owned->reader, line, sizeof(line), 9000);
	assert(snprintf(expected, sizeof(expected), "W %d\n", status) > 0);
	assert(strcmp(line, expected) == 0);
	assert(read(owned->reader, &byte, 1) == 0);
	assert(waitpid(owned->pid, &child_status, 0) == owned->pid);
	assert(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
	close(owned->reader);
	if (owned->writer >= 0)
		close(owned->writer);
}

static void test_writer_order(const char *path, bool parent_first)
{
	struct owned_guardian owned = start_guardian(path, "normal");
	char line[128];
	int core_writer = dup(owned.writer);
	struct pollfd descriptor = {owned.reader, POLLIN, 0};

	assert(core_writer >= 0);
	read_line(owned.reader, line, sizeof(line), 6000);
	assert(strcmp(line, normal_address) == 0);
	if (parent_first)
	{
		close(owned.writer);
		owned.writer = -1;
	}
	else
		close(core_writer);
	/* A surviving writer keeps the direct daemon alive without wake polling. */
	assert(poll(&descriptor, 1, 100) == 0);
	if (parent_first)
		close(core_writer);
	else
	{
		close(owned.writer);
		owned.writer = -1;
	}
	expect_wait(&owned, 128 + SIGTERM);
}

static void test_failures(const char *path)
{
	struct owned_guardian owned;
	char line[128];
	int status;
	const char *invalid[] = {"malformed", "trailing", "stall"};
	size_t i;

	owned = start_guardian(path, "early");
	expect_wait(&owned, 23);
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		owned = start_guardian(path, invalid[i]);
		expect_wait(&owned, 128 + SIGTERM);
	}
	owned = start_guardian(path, "normal");
	close(owned.writer);
	owned.writer = -1;
	/* EOF before startup is permitted to produce only the terminal record. */
	read_line(owned.reader, line, sizeof(line), 6000);
	if (strcmp(line, normal_address) == 0)
		read_line(owned.reader, line, sizeof(line), 6000);
	assert(snprintf(line + 32, sizeof(line) - 32,
		"W %d\n", 128 + SIGTERM) > 0);
	assert(strcmp(line, line + 32) == 0);
	assert(read(owned.reader, line, 1) == 0);
	assert(waitpid(owned.pid, &status, 0) == owned.pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	close(owned.reader);
	owned = start_guardian(path, "refuse");
	read_line(owned.reader, line, sizeof(line), 6000);
	assert(strcmp(line, normal_address) == 0);
	close(owned.writer);
	owned.writer = -1;
	expect_wait(&owned, 128 + SIGKILL);
	/* An observer lost before address delivery is a failed startup. */
	owned = start_guardian(path, "stall");
	close(owned.reader);
	assert(waitpid(owned.pid, &status, 0) == owned.pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	close(owned.writer);
	/* Parent receipt loss cannot override a surviving core lifetime writer. */
	owned = start_guardian(path, "normal");
	read_line(owned.reader, line, sizeof(line), 6000);
	close(owned.reader);
	assert(poll(NULL, 0, 100) == 0);
	assert(waitpid(owned.pid, &status, WNOHANG) == 0);
	close(owned.writer);
	assert(waitpid(owned.pid, &status, 0) == owned.pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void test_maximum(const char *path)
{
	struct owned_guardian owned = start_guardian(path, "maximum");
	char address[4098];

	/* Read a full >PIPE_BUF record through the partial-aware output path. */
	read_line(owned.reader, address, sizeof(address), 6000);
	assert(strlen(address) == 4097);
	assert(strncmp(address, "unix:path=/", 11) == 0);
	close(owned.writer);
	owned.writer = -1;
	expect_wait(&owned, 128 + SIGTERM);
}

static void test_refusals(const char *path)
{
	char *arguments[] = {"guardian-unit", "bus-guardian", (char *)path,
		"--arbitrary", NULL};
	int status;
	pid_t child = fork();

	assert(child >= 0);
	if (child == 0)
	{
		int regular = open("/dev/null", O_RDONLY);
		assert(regular >= 0);
		assert(dup2(regular, 3) == 3);
		assert(session_bus_guardian_main(4, arguments) == 2);
		assert(session_bus_guardian_main(3, arguments) == 2);
		arguments[2] = "dbus-daemon";
		assert(session_bus_guardian_main(3, arguments) == 2);
		arguments[2] = "/bin/sh";
		assert(session_bus_guardian_main(3, arguments) == 2);
		_exit(0);
	}
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(int argc, char **argv)
{
	char directory[] = "/tmp/fvwm-guardian-unit-XXXXXX";
	char path[sizeof(directory) + 20];
	char *binary;
	const char *base = strrchr(argv[0], '/');

	if (base && strcmp(base + 1, "dbus-daemon") == 0)
		return fake_daemon(argc, argv);
	/* Prevent a failed lifecycle assertion from leaving a unit hung forever. */
	alarm(40);
	binary = realpath(argv[0], NULL);
	assert(binary != NULL);
	assert(mkdtemp(directory) != NULL);
	assert(snprintf(path, sizeof(path), "%s/dbus-daemon", directory) > 0);
	assert(symlink(binary, path) == 0);
	free(binary);
	test_writer_order(path, true);
	test_writer_order(path, false);
	test_failures(path);
	test_maximum(path);
	test_refusals(path);
	assert(unlink(path) == 0);
	assert(rmdir(directory) == 0);
	return 0;
}
