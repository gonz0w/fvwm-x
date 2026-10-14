/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_process.h"
#include "libs/fvwmsignal.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t owned_child;
static unsigned kill_calls;
static bool reject_sink;
static size_t consumed;
static size_t largest_chunk;

pid_t __real_fork(void);
int __real_kill(pid_t child, int signal_number);

pid_t __wrap_fork(void)
{
	owned_child = __real_fork();
	return owned_child;
}

int __wrap_kill(pid_t child, int signal_number)
{
	kill_calls++;
	return __real_kill(child, signal_number);
}

static bool consume(const unsigned char *bytes, size_t length, void *context)
{
	const image_input *source = context;

	if (reject_sink)
	{
		errno = EPROTO;
		return false;
	}
	assert(length <= source->length - consumed);
	assert(memcmp(bytes, source->bytes + consumed, length) == 0);
	consumed += length;
	if (length > largest_chunk)
	{
		largest_chunk = length;
	}
	return true;
}

static int fd_count(void)
{
	int count = 0;

	for (int fd = 0; fd < 256; fd++)
	{
		count += fcntl(fd, F_GETFD) >= 0 || errno != EBADF;
	}
	return count;
}

static image_process *start(const char *program, char *mode,
	const image_input *source, size_t limit, int timeout)
{
	char *args[] = {(char *)program, mode, NULL};

	consumed = largest_chunk = 0;
	return image_process_start(source, program, args, limit, timeout, 0,
	    consume, (void *)source);
}

static image_process_status pump(image_process *process)
{
	image_process_status result;

	for (unsigned turns = 0;; turns++)
	{
		assert(turns < 10000);
		result = image_process_step(process);
		if (result != IMAGE_PROCESS_RUNNING)
		{
			return result;
		}
		struct pollfd endpoints[2];

		image_process_descriptors(process, endpoints);
		int timeout = image_process_timeout_ms(process);

		assert(timeout >= 0 && timeout <= 3000);
		int ready = poll(endpoints, 2, timeout > 20 ? 20 : timeout);

		assert(ready >= 0 || errno == EINTR);
	}
}

static void released(image_process *process, int descriptors)
{
	assert(image_process_destroy(process));
	assert(fd_count() == descriptors);
	assert(fvwm_child_watch_available());
	assert(waitpid(owned_child, NULL, WNOHANG) == -1 && errno == ECHILD);
}

static int worker(char *mode)
{
	if (strcmp(mode, "stall") == 0)
	{
		for (;;)
		{
			pause();
		}
	}
	if (strcmp(mode, "closed-stall") == 0)
	{
		close(STDOUT_FILENO);
		for (;;)
		{
			pause();
		}
	}
	if (strcmp(mode, "exit") == 0 || strcmp(mode, "failed") == 0)
	{
		return mode[0] == 'e' ? 0 : 19;
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

static void test_duplex(const char *program, const image_input *source)
{
	int descriptors = fd_count();
	image_process *process = start(program, "copy", source,
	    source->length, 3000);

	assert(process != NULL);
	assert(!image_process_destroy(process) && errno == EBUSY);
	assert(pump(process) == IMAGE_PROCESS_SUCCEEDED);
	assert(consumed == source->length && largest_chunk <= 65536);
	released(process, descriptors);
}

static void test_failure(const char *program, char *mode,
	const image_input *source, size_t limit, int timeout, int error)
{
	int descriptors = fd_count();
	image_process *process = start(program, mode, source, limit, timeout);

	assert(process != NULL);
	assert(pump(process) == IMAGE_PROCESS_FAILED);
	assert(image_process_error(process) == error);
	released(process, descriptors);
}

static void test_other_child_service(const char *program,
	const image_input *source)
{
	int descriptors = fd_count();
	image_process *process = start(program, "stall", source,
	    source->length, 3000);

	assert(process != NULL);
	pid_t decoder = owned_child;
	image_process *other = start(program, "copy", source,
	    source->length, 3000);

	assert(other == NULL && errno == EBUSY);
	owned_child = decoder;
	int marker[2];

	assert(pipe(marker) == 0);
	pid_t independent = __real_fork();

	assert(independent >= 0);
	if (independent == 0)
	{
		close(marker[0]);
		struct timespec delay = {0, 50000000};

		nanosleep(&delay, NULL);
		assert(write(marker[1], "ready", 5) == 5);
		_exit(0);
	}
	close(marker[1]);
	struct pollfd readiness = {marker[0], POLLIN, 0};
	int ready;

	do
	{
		assert(image_process_step(process) == IMAGE_PROCESS_RUNNING);
		ready = poll(&readiness, 1, 100);
	} while (ready == 0 || (ready < 0 && errno == EINTR));
	assert(ready == 1);
	char bytes[5];

	assert(read(marker[0], bytes, sizeof(bytes)) == 5);
	assert(memcmp(bytes, "ready", 5) == 0);
	assert(image_process_step(process) == IMAGE_PROCESS_RUNNING);
	close(marker[0]);
	/* Independent child notifications can run while the decoder remains
	 * owned and pending. No process-wide SIGCHLD mask spans that wait. */
	for (unsigned i = 0; i < 100 && __real_kill(independent, 0) == 0; i++)
	{
		struct timespec delay = {0, 1000000};

		nanosleep(&delay, NULL);
	}
	assert(waitpid(independent, NULL, WNOHANG) == -1 && errno == ECHILD);
	image_process_cancel(process);
	assert(pump(process) == IMAGE_PROCESS_FAILED);
	assert(image_process_error(process) == ECANCELED);
	released(process, descriptors);
}

static void test_reaped_cancel(const char *program)
{
	int descriptors = fd_count();
	image_input empty = {0};
	image_process *process = start(program, "exit", &empty, 16, 3000);

	assert(process != NULL);
	int status;
	bool captured = false;

	for (unsigned i = 0; i < 1000 && !captured; i++)
	{
		sigset_t blocked, previous;

		sigemptyset(&blocked);
		sigaddset(&blocked, SIGCHLD);
		assert(sigprocmask(SIG_BLOCK, &blocked, &previous) == 0);
		captured = fvwm_child_status(owned_child, &status);
		assert(sigprocmask(SIG_SETMASK, &previous, NULL) == 0);
		struct timespec delay = {0, 1000000};

		nanosleep(&delay, NULL);
	}
	assert(captured && WIFEXITED(status) && WEXITSTATUS(status) == 0);
	unsigned before = kill_calls;

	image_process_cancel(process);
	assert(kill_calls == before);
	assert(pump(process) == IMAGE_PROCESS_FAILED);
	assert(image_process_error(process) == ECANCELED);
	released(process, descriptors);
}

static void test_shutdown_handoff(const char *program,
	const image_input *source)
{
	int descriptors = fd_count();
	image_process *process = start(program, "stall", source,
	    source->length, 3000);
	struct sigaction action = {0};

	assert(process != NULL);
	action.sa_handler = SIG_DFL;
	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGCHLD, &action, NULL) == 0);
	assert(!image_process_handoff(process) && errno == EINVAL);
	assert(!image_process_destroy(process) && errno == EBUSY);
	action.sa_handler = fvwmReapChildren;
	assert(sigaction(SIGCHLD, &action, NULL) == 0);
	assert(image_process_handoff(process));
	assert(fd_count() == descriptors);
	assert(fvwm_child_watch_available());
	for (unsigned i = 0; i < 1000 && __real_kill(owned_child, 0) == 0; i++)
	{
		struct timespec delay = {0, 1000000};

		nanosleep(&delay, NULL);
	}
	assert(waitpid(owned_child, NULL, WNOHANG) == -1 && errno == ECHILD);
}

int main(int argc, char **argv)
{
	if (argc > 1)
	{
		return worker(argv[1]);
	}
	alarm(20);
	char *program = realpath(argv[0], NULL);
	image_input source = {malloc(1048576), 1048576};
	struct sigaction action = {0}, previous;

	assert(program != NULL && source.bytes != NULL);
	for (size_t i = 0; i < source.length; i++)
	{
		source.bytes[i] = (unsigned char)i;
	}
	action.sa_handler = fvwmReapChildren;
	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGCHLD, &action, &previous) == 0);
	test_duplex(program, &source);
	test_failure(program, "copy", &source, 8192, 3000, EFBIG);
	test_failure(program, "stall", &source, source.length, 100, ETIMEDOUT);
	test_failure(program, "closed-stall", &source, source.length,
	    100, ETIMEDOUT);
	reject_sink = true;
	test_failure(program, "copy", &source, source.length, 3000, EPROTO);
	reject_sink = false;
	image_input empty = {0};

	test_failure(program, "failed", &empty, 16, 3000, EIO);
	test_other_child_service(program, &source);
	test_reaped_cancel(program);
	test_shutdown_handoff(program, &source);
	action.sa_handler = SIG_IGN;
	assert(sigaction(SIGCHLD, &action, NULL) == 0);
	assert(start(program, "copy", &source, source.length, 3000) == NULL);
	assert(errno == EINVAL);
	action.sa_handler = SIG_DFL;
	assert(sigaction(SIGCHLD, &action, NULL) == 0);
	test_duplex(program, &source);
	assert(sigaction(SIGCHLD, &previous, NULL) == 0);
	image_input_destroy(&source);
	free(program);
	puts("incremental image transport preserves readiness "
	    "and child ownership");
	return 0;
}
