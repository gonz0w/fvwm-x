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
	session_ready_publish(&descriptor);
	assert(descriptor == -1);
	assert(fcntl(3, F_GETFD) == -1 && errno == EBADF);
	assert(read(reader, bytes, sizeof(bytes)) == 1 && bytes[0] == 'R');
	session_ready_publish(&descriptor);
	assert(read(reader, bytes, sizeof(bytes)) == 0);
	close(reader);
}

static void test_full_pipe_and_missing_reader(void)
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
	session_ready_publish(&descriptor);
	assert(descriptor == -1);
	close(reader);
	reader = make_ready_pipe();
	descriptor = session_ready_take();
	close(reader);
	memset(&action, 0, sizeof(action));
	sigemptyset(&action.sa_mask);
	action.sa_handler = SIG_DFL;
	assert(sigaction(SIGPIPE, &action, NULL) == 0);
	session_ready_publish(&descriptor);
	assert(descriptor == -1);
	assert(sigaction(SIGPIPE, NULL, &after) == 0);
	assert(after.sa_handler == SIG_DFL);
}

int main(void)
{
	test_environment_and_refusals();
	test_one_shot_flags_and_bytes();
	test_full_pipe_and_missing_reader();
	return 0;
}
