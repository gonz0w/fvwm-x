/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_decompress.h"
#include "libs/image_process.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t owned_child;
static int fail_fork, fail_allocate;
static volatile sig_atomic_t pipe_signals, wrongly_reaped;

pid_t __real_fork(void);
void *__real_realloc(void *bytes, size_t count);

pid_t
__wrap_fork(void)
{
	if (fail_fork)
	{
		errno = EAGAIN;
		return -1;
	}
	owned_child = __real_fork();
	return owned_child;
}

void *
__wrap_realloc(void *bytes, size_t count)
{
	if (fail_allocate)
	{
		errno = ENOMEM;
		return NULL;
	}
	return __real_realloc(bytes, count);
}

static void
pipe_signal(int signal_number)
{
	(void)signal_number;
	pipe_signals++;
}

static void
reap_signal(int signal_number)
{
	pid_t child;

	(void)signal_number;
	while ((child = waitpid(-1, NULL, WNOHANG)) > 0)
	{
		if (child == owned_child)
		{
			wrongly_reaped++;
		}
	}
}

static int
probe(int argc, char **argv)
{
	if (strcmp(argv[1], "early") == 0)
	{
		return 0;
	}
	if (strcmp(argv[1], "stall") == 0 ||
	    strcmp(argv[1], "closed-output-stall") == 0)
	{
		if (argv[1][0] == 'c')
		{
			close(STDOUT_FILENO);
		}
		for (;;)
		{
			pause();
		}
	}
	if (strcmp(argv[1], "fail") == 0)
	{
		return 23;
	}
	if (strcmp(argv[1], "inspect") == 0)
	{
		assert(argc == 3);
		assert(fcntl(atoi(argv[2]), F_GETFD) == -1 && errno == EBADF);
	}
	unsigned char bytes[4096];
	ssize_t count;

	while ((count = read(STDIN_FILENO, bytes, sizeof(bytes))) > 0)
	{
		size_t used = 0;

		while (used < (size_t)count)
		{
			ssize_t sent = write(STDOUT_FILENO, bytes + used,
			    (size_t)count - used);

			assert(sent > 0);
			used += (size_t)sent;
		}
	}
	assert(count == 0);
	return 0;
}

static bool
cancelled(void *context)
{
	int *calls = context;

	return ++*calls >= 3;
}

static int
count_descriptors(void)
{
	int count = 0;

	for (int fd = 0; fd < 256; fd++)
	{
		if (fcntl(fd, F_GETFD) >= 0 || errno != EBADF)
		{
			count++;
		}
	}
	return count;
}

static int
load(const char *program, char *const args[], const image_input *source,
	size_t limit, int timeout, bool cancellation)
{
	image_input output = {0};
	int calls = 0, descriptors = count_descriptors();

	owned_child = -1;
	bool success = image_decompress(source, program, args, limit, timeout,
	    cancellation ? cancelled : NULL, &calls, &output);
	int error = errno;

	if (success)
	{
		assert(output.length == source->length);
		assert(output.bytes[output.length] == 0);
		assert(memcmp(output.bytes, source->bytes, output.length) == 0);
	}
	else
	{
		assert(output.bytes == NULL && output.length == 0);
		assert(error != 0);
	}
	image_input_destroy(&output);
	assert(count_descriptors() == descriptors);
	if (owned_child > 0)
	{
		assert(waitpid(owned_child, NULL, WNOHANG) == -1 &&
		    errno == ECHILD);
		assert(kill(owned_child, 0) == -1 && errno == ESRCH);
	}
	if (pipe_signals != 0 || wrongly_reaped != 0)
	{
		fprintf(stderr, "mode=%s pipe_signals=%d wrongly_reaped=%d\n",
		    args[1], (int)pipe_signals, (int)wrongly_reaped);
	}
	assert(pipe_signals == 0 && wrongly_reaped == 0);
	return success ? 0 : error;
}

static void
test_closed_stdio(const char *program, char *const args[],
	const image_input *source)
{
	pid_t harness = __real_fork();

	assert(harness >= 0);
	if (harness == 0)
	{
		close(0);
		close(1);
		close(2);
		_exit(load(program, args, source, source->length, 3000, false)
		    == 0 ? 0 : 1);
	}
	int status;

	assert(waitpid(harness, &status, 0) == harness);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void
test_async_collector(const char *program, char *const args[],
	const image_input *source, size_t limit)
{
	struct sigaction previous, action = {.sa_handler = SIG_DFL};
	image_decompress_result result = {0};
	int descriptors = count_descriptors();

	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGCHLD, &action, &previous) == 0);
	image_process *process = image_decompress_start(source, program,
	    args, limit, 3000, &result);

	assert(process != NULL && result.output.bytes == NULL);
	image_process_status status;

	while ((status = image_process_step(process)) == IMAGE_PROCESS_RUNNING)
	{
		struct pollfd endpoints[2];
		int timeout = image_process_timeout_ms(process);

		image_process_descriptors(process, endpoints);
		assert(poll(endpoints, 2, timeout > 10 ? 10 : timeout) >= 0 ||
		    errno == EINTR);
	}
	if (source->length <= limit)
	{
		assert(status == IMAGE_PROCESS_SUCCEEDED);
		assert(result.output.length == source->length);
		if (source->length != 0)
		{
			assert(memcmp(result.output.bytes, source->bytes,
			    source->length) == 0);
			assert(result.output.bytes[source->length] == 0);
		}
	}
	else
	{
		assert(status == IMAGE_PROCESS_FAILED);
		assert(image_process_error(process) == EFBIG);
		assert(result.output.length <= limit);
	}
	assert(image_process_destroy(process));
	image_decompress_result_destroy(&result);
	assert(result.output.bytes == NULL && result.capacity == 0);
	assert(count_descriptors() == descriptors);
	assert(sigaction(SIGCHLD, &previous, NULL) == 0);
}

int
main(int argc, char **argv)
{
	if (argc > 1)
	{
		return probe(argc, argv);
	}
	alarm(15);
	char *program = realpath(argv[0], NULL);
	image_input source = {malloc(200000), 200000};
	struct sigaction old_pipe, old_child, action = {0};

	assert(program != NULL && source.bytes != NULL);
	for (size_t i = 0; i < source.length; i++)
	{
		source.bytes[i] = (unsigned char)i;
	}
	action.sa_handler = pipe_signal;
	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGPIPE, &action, &old_pipe) == 0);
	action.sa_handler = reap_signal;
	assert(sigaction(SIGCHLD, &action, &old_child) == 0);
	char *copy[] = {program, "copy", NULL};

	assert(load(program, copy, &source, source.length, 3000, false) == 0);
	assert(load(program, copy, &source, 8192, 3000, false) == EFBIG);
	fail_fork = 1;
	assert(load(program, copy, &source, source.length, 3000, false)
	    == EAGAIN);
	fail_fork = 0;
	fail_allocate = 1;
	assert(load(program, copy, &source, source.length, 3000, false)
	    == ENOMEM);
	fail_allocate = 0;
	char *stall[] = {program, "stall", NULL};
	char *closed[] = {program, "closed-output-stall", NULL};
	char *early[] = {program, "early", NULL};
	char *failed[] = {program, "fail", NULL};

	assert(load(program, stall, &source, source.length, 200, false)
	    == ETIMEDOUT);
	assert(load(program, closed, &source, source.length, 200, false)
	    == ETIMEDOUT);
	assert(load(program, stall, &source, source.length, 3000, true)
	    == ECANCELED);
	assert(load(program, early, &source, source.length, 3000, false) != 0);
	assert(load(program, failed, &source, source.length, 3000, false) != 0);
	assert(load("/fvwm-image-test-no-executable", copy, &source,
	    source.length, 3000, false) != 0);
	int inherited = fcntl(STDIN_FILENO, F_DUPFD, 70);

	assert(inherited >= 70);
	char number[32];

	snprintf(number, sizeof(number), "%d", inherited);
	char *inspect[] = {program, "inspect", number, NULL};

	assert(load(program, inspect, &source, source.length, 3000, false)
	    == 0);
	assert(close(inherited) == 0);
	/* Keep caller-owned pending SIGPIPE when a helper write fails. */
	sigset_t blocked, previous, pending;

	sigemptyset(&blocked);
	sigaddset(&blocked, SIGPIPE);
	assert(sigprocmask(SIG_BLOCK, &blocked, &previous) == 0);
	assert(raise(SIGPIPE) == 0);
	assert(load(program, early, &source, source.length, 3000, false) != 0);
	assert(sigpending(&pending) == 0 &&
	    sigismember(&pending, SIGPIPE) == 1);
	int received;

	int pending_pipe_count = 0;

	/* BSD can retain the fixture's process-directed signal separately from
	 * the write's thread-directed signal. Both belong to this test; drain
	 * both before restoring the mask; do not assume one queue slot. */
	while (sigismember(&pending, SIGPIPE) == 1)
	{
		assert(++pending_pipe_count <= 2);
		assert(sigwait(&blocked, &received) == 0 &&
		    received == SIGPIPE);
		assert(sigpending(&pending) == 0);
	}
	printf("preserved and drained %d owned pending SIGPIPE signals\n",
	    pending_pipe_count);
	assert(sigprocmask(SIG_SETMASK, &previous, NULL) == 0);
	/* Preserve another child's waitable state and deferred reaper work. */
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGCHLD);
	assert(sigprocmask(SIG_BLOCK, &blocked, &previous) == 0);
	pid_t unrelated = __real_fork();

	assert(unrelated >= 0);
	if (unrelated == 0)
	{
		_exit(0);
	}
	assert(sigwait(&blocked, &received) == 0 && received == SIGCHLD);
	assert(raise(SIGCHLD) == 0);
	assert(load(program, copy, &source, source.length, 3000, false) == 0);
	assert(sigprocmask(SIG_SETMASK, &previous, NULL) == 0);
	assert(waitpid(unrelated, NULL, WNOHANG) == -1 && errno == ECHILD);
	assert(wrongly_reaped == 0);
	/* Module callers may have automatic reaping. */
	action.sa_handler = SIG_IGN;
	assert(sigaction(SIGCHLD, &action, NULL) == 0);
	assert(load(program, copy, &source, source.length, 3000, false) == 0);
	struct sigaction restored;

	assert(sigaction(SIGCHLD, NULL, &restored) == 0);
	assert(restored.sa_handler == SIG_IGN);
	action.sa_handler = reap_signal;
	action.sa_flags = SA_NOCLDWAIT;
	assert(sigaction(SIGCHLD, &action, NULL) == 0);
	assert(load(program, copy, &source, source.length, 3000, false) == 0);
	assert(sigaction(SIGCHLD, NULL, &restored) == 0);
	assert(restored.sa_handler == reap_signal);
	assert(restored.sa_flags & SA_NOCLDWAIT);
	assert(sigaction(SIGCHLD, &old_child, NULL) == 0);
	test_closed_stdio(program, copy, &source);
	image_input empty = {(unsigned char *)"", 0};

	assert(load(program, copy, &empty, 8, 3000, false) == 0);
	test_async_collector(program, copy, &source, source.length);
	test_async_collector(program, copy, &source, 8192);
	test_async_collector(program, copy, &empty, 8);
	image_input output = {0};

	assert(!image_decompress(&source, program, copy, 0, 3000,
	    NULL, NULL, &output) && errno == EINVAL);
	assert(!image_decompress(&source, program, copy, 8, 0,
	    NULL, NULL, &output) && errno == EINVAL);
	assert(!image_decompress(&source, program, copy, 8, 60001,
	    NULL, NULL, &output) && errno == EINVAL);
	assert(sigaction(SIGPIPE, &old_pipe, NULL) == 0);
	image_input_destroy(&source);
	free(program);
	puts("bounded duplex decompression owns child, bytes, deadline "
	    "and signals");
	return 0;
}
