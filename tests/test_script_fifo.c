/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/script_fifo.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static char directory[] = "/tmp/fvwm-fifo-XXXXXX";
static char input[512], output[512];
static volatile sig_atomic_t interrupted;

static double seconds(void)
{
	struct timespec now;
	assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
	return now.tv_sec + now.tv_nsec / 1000000000.0;
}

static void pause_ms(int ms)
{
	struct timespec duration = {ms / 1000, (ms % 1000) * 1000000L};
	while (nanosleep(&duration, &duration) < 0)
		assert(errno == EINTR);
}

static void endpoints(void)
{
	unlink(input);
	unlink(output);
	assert(mkfifo(input, 0600) == 0);
	assert(mkfifo(output, 0600) == 0);
}

static pid_t helper(const char *reply, size_t length, int startup_ms,
	int answer_ms, bool immediate, int *ready)
{
	int barrier[2];
	assert(pipe(barrier) == 0);
	pid_t child = fork();
	assert(child >= 0);
	if (child == 0)
	{
		close(barrier[0]);
		pause_ms(startup_ms);
		int fd = open(input, O_RDONLY | (immediate ? O_NONBLOCK : 0));
		assert(fd >= 0);
		assert(write(barrier[1], "r", 1) == 1);
		close(barrier[1]);
		char bytes[100];
		size_t used = 0;
		for (;;)
		{
			struct pollfd p = {fd, POLLIN, 0};
			assert(poll(&p, 1, 2000) > 0);
			ssize_t n = read(fd, bytes + used, sizeof(bytes) - used);
			assert(n >= 0);
			used += n;
			if (n == 0)
				break;
		}
		assert(used == 5 && memcmp(bytes, "hello", 5) == 0);
		close(fd);
		pause_ms(answer_ms);
		if (reply != NULL)
		{
			fd = open(output, O_WRONLY);
			assert(fd >= 0);
			size_t sent = 0;
			while (sent < length)
			{
				ssize_t n = write(fd, reply + sent, length - sent);
				assert(n > 0);
				sent += n;
			}
			close(fd);
		}
		_exit(0);
	}
	close(barrier[1]);
	*ready = barrier[0];
	return child;
}

static void reaped(pid_t child)
{
	int status;
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static bool close_stdin(void *context)
{
	bool *closed = context;
	if (!*closed)
	{
		assert(close(STDIN_FILENO) == 0);
		*closed = true;
	}
	return false;
}

static void test_replies_and_modes(void)
{
	char *reply;
	size_t length;
	for (int kind = 0; kind < 3; kind++)
	{
		endpoints();
		char *bytes = malloc(SCRIPT_FIFO_REPLY_MAX_BYTES);
		assert(bytes != NULL);
		memset(bytes, 'x', SCRIPT_FIFO_REPLY_MAX_BYTES);
		size_t wanted = kind == 0 ? 0 : kind == 1 ? 7 :
			SCRIPT_FIFO_REPLY_MAX_BYTES;
		int ready;
		pid_t child = helper(bytes, wanted, 30, 30, false, &ready);
		assert(script_fifo_exchange(directory, "case", "hello", 1, 2000,
			NULL, NULL, &reply, &length));
		assert(length == wanted && reply[length] == '\0');
		assert(memcmp(reply, bytes, length) == 0);
		free(reply);
		free(bytes);
		close(ready);
		reaped(child);
	}
	for (int mode = 0; mode <= 2; mode += 2)
	{
		endpoints();
		int ready;
		pid_t child = helper(NULL, 0, 0, 0, mode == 2, &ready);
		if (mode == 2)
		{
			char byte;
			assert(read(ready, &byte, 1) == 1);
		}
		int saved_stdin = dup(STDIN_FILENO);
		assert(saved_stdin >= 0);
		bool closed = false;
		assert(script_fifo_exchange(directory, "case", "hello", mode,
			2000, close_stdin, &closed, &reply, &length));
		assert(closed && reply == NULL && length == 0);
		if (mode == 0)
		{
			char byte;
			assert(read(ready, &byte, 1) == 1);
		}
		assert(dup2(saved_stdin, STDIN_FILENO) == STDIN_FILENO);
		close(saved_stdin);
		close(ready);
		reaped(child);
	}
}

static void alarm_received(int number)
{
	(void)number;
	interrupted++;
}

static bool cancelled(void *context)
{
	(void)context;
	return interrupted != 0;
}

static void test_timeouts_and_interruption(void)
{
	endpoints();
	char *reply = (char *)1;
	size_t length = 123;
	double start = seconds();
	assert(!script_fifo_exchange(directory, "case", "hello", 0, 100,
		NULL, NULL, &reply, &length));
	assert(errno == ETIMEDOUT && reply == NULL && length == 0);
	assert(seconds() - start >= 0.09 && seconds() - start < 1.5);
	start = seconds();
	assert(!script_fifo_exchange(directory, "case", "hello", 2, 5000,
		NULL, NULL, &reply, &length));
	assert(errno == ENXIO && seconds() - start < 0.5);
	/* A present read endpoint without a writer must not become an empty reply. */
	int ready;
	pid_t child = helper(NULL, 0, 0, 0, false, &ready);
	assert(!script_fifo_exchange(directory, "case", "hello", 1, 150,
		NULL, NULL, &reply, &length));
	assert(errno == ETIMEDOUT && reply == NULL && length == 0);
	close(ready);
	reaped(child);
	/* Pipe backpressure shares the same deadline and cannot grow progress
	 * backwards on a failed write. Only the test fixture drains no data. */
	int reader = open(input, O_RDONLY | O_NONBLOCK);
	assert(reader >= 0);
	char *large = malloc(1024 * 1024 + 1);
	assert(large != NULL);
	memset(large, 'x', 1024 * 1024);
	large[1024 * 1024] = '\0';
	assert(!script_fifo_exchange(directory, "case", large, 0, 100,
		NULL, NULL, &reply, &length));
	assert(errno == ETIMEDOUT);
	assert(!script_fifo_exchange(directory, "case", large, 2, 5000,
		NULL, NULL, &reply, &length));
	assert(errno == EMSGSIZE);
	free(large);
	close(reader);
	struct sigaction action = {0};
	action.sa_handler = alarm_received;
	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGALRM, &action, NULL) == 0);
	struct itimerval timer = {{0, 5000}, {0, 5000}};
	assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
	start = seconds();
	assert(!script_fifo_exchange(directory, "case", "hello", 0, 100,
		NULL, NULL, &reply, &length));
	assert(errno == ETIMEDOUT && interrupted > 0);
	assert(seconds() - start >= 0.09 && seconds() - start < 1.5);
	assert(!script_fifo_exchange(directory, "case", "hello", 0, 5000,
		cancelled, NULL, &reply, &length));
	assert(errno == ECANCELED);
	timer = (struct itimerval){{0, 0}, {0, 0}};
	assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
}

static bool disconnect_reader(void *context)
{
	int *reader = context;
	for (int fd = 0; fd < 64 && *reader >= 0; fd++)
	{
		struct stat a, b;
		if (fd != *reader && fstat(fd, &a) == 0 &&
		    fstat(*reader, &b) == 0 && a.st_dev == b.st_dev &&
		    a.st_ino == b.st_ino &&
		    (fcntl(fd, F_GETFL) & O_ACCMODE) == O_WRONLY)
		{
			assert(fcntl(fd, F_GETFD) & FD_CLOEXEC);
			assert(close(*reader) == 0);
			*reader = -1;
		}
	}
	return false;
}

static void test_broken_peer_and_overall_deadline(void)
{
	endpoints();
	int reader = open(input, O_RDONLY | O_NONBLOCK);
	assert(reader >= 0);
	char *reply;
	size_t length;
	struct sigaction before, after;
	sigset_t mask_before, mask_after;
	assert(sigaction(SIGPIPE, NULL, &before) == 0);
	assert(sigprocmask(SIG_BLOCK, NULL, &mask_before) == 0);
	assert(!script_fifo_exchange(directory, "case", "hello", 0, 100,
		disconnect_reader, &reader, &reply, &length));
	assert(errno == EPIPE && reader == -1 && reply == NULL && length == 0);
	assert(sigaction(SIGPIPE, NULL, &after) == 0);
	assert(sigprocmask(SIG_BLOCK, NULL, &mask_after) == 0);
	assert(before.sa_handler == after.sa_handler);
	assert(sigismember(&mask_before, SIGPIPE) ==
		sigismember(&mask_after, SIGPIPE));
	/* A previously pending signal belongs to the caller and is retained. */
	sigset_t block, previous, pending;
	sigemptyset(&block);
	sigaddset(&block, SIGPIPE);
	assert(sigprocmask(SIG_BLOCK, &block, &previous) == 0);
	assert(raise(SIGPIPE) == 0);
	reader = open(input, O_RDONLY | O_NONBLOCK);
	assert(reader >= 0);
	assert(!script_fifo_exchange(directory, "case", "hello", 0, 100,
		disconnect_reader, &reader, &reply, &length));
	assert(errno == EPIPE && reader == -1);
	assert(sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 1);
	/* FreeBSD may retain both the fixture's raised signal and the write's
	 * signal; other systems coalesce them. Drain only this test's signals
	 * before restoring its mask, after checking the caller's is retained. */
	int received, consumed = 0;
	do
	{
		assert(++consumed <= 2);
		assert(sigwait(&block, &received) == 0 && received == SIGPIPE);
		assert(sigpending(&pending) == 0);
	} while (sigismember(&pending, SIGPIPE) == 1);
	assert(sigprocmask(SIG_SETMASK, &previous, NULL) == 0);
	int ready;
	pid_t child = helper("answer", 6, 25, 200, false, &ready);
	double start = seconds();
	assert(!script_fifo_exchange(directory, "case", "hello", 1, 150,
		NULL, NULL, &reply, &length));
	assert(errno == ETIMEDOUT && seconds() - start < 1.5);
	/* Transport does not own the helper. This test alone cancels its own
	 * fixture after verifying the helper survived the expired call. */
	assert(kill(child, 0) == 0);
	assert(kill(child, SIGTERM) == 0);
	int status;
	assert(waitpid(child, &status, 0) == child && WIFSIGNALED(status));
	close(ready);
}

static void test_refusal_and_text(void)
{
	char *reply;
	size_t length;
	assert(!script_fifo_exchange(directory, "../case", "hello", 0, 100,
		NULL, NULL, &reply, &length) && errno == EINVAL);
	assert(!script_fifo_exchange(directory, "case", "hello", 3, 100,
		NULL, NULL, &reply, &length) && errno == EINVAL);
	assert(!script_fifo_exchange(directory, "case", "hello", 0, 0,
		NULL, NULL, &reply, &length) && errno == EINVAL);
	endpoints();
	assert(unlink(input) == 0);
	int regular = open(input, O_CREAT | O_RDWR, 0600);
	assert(regular >= 0);
	assert(write(regular, "unchanged", 9) == 9);
	assert(!script_fifo_exchange(directory, "case", "hello", 0, 100,
		NULL, NULL, &reply, &length) && errno == EPERM);
	char bytes[9];
	assert(lseek(regular, 0, SEEK_SET) == 0 && read(regular, bytes, 9) == 9);
	assert(memcmp(bytes, "unchanged", 9) == 0);
	close(regular);
	assert(unlink(input) == 0 && symlink(output, input) == 0);
	assert(!script_fifo_exchange(directory, "case", "hello", 2, 100,
		NULL, NULL, &reply, &length) && errno == EPERM);
	endpoints();
	if (getuid() == 0)
	{
		assert(chown(input, 65534, (gid_t)-1) == 0);
		assert(!script_fifo_exchange(directory, "case", "hello", 0, 100,
			NULL, NULL, &reply, &length) && errno == EPERM);
		assert(chown(input, 0, (gid_t)-1) == 0);
	}
	int ready;
	pid_t child = helper("x\0y", 3, 0, 0, false, &ready);
	assert(!script_fifo_exchange(directory, "case", "hello", 1, 2000,
		NULL, NULL, &reply, &length));
	assert(errno == EINVAL && reply == NULL && length == 0);
	close(ready);
	reaped(child);
}

int main(void)
{
	assert(mkdtemp(directory) != NULL);
	snprintf(input, sizeof(input), "%s/.tmp-com-in-case", directory);
	snprintf(output, sizeof(output), "%s/.tmp-com-out-case", directory);
	test_replies_and_modes();
	test_timeouts_and_interruption();
	test_broken_peer_and_overall_deadline();
	test_refusal_and_text();
	unlink(input);
	unlink(output);
	assert(rmdir(directory) == 0);
	puts("script FIFO deadlines, transport and endpoint policy: passed");
	return 0;
}
