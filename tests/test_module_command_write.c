/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Module.h"
#include "libs/defaults.h"
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

#define FIXTURE_FD 4242
static unsigned char captured[32 * (3 * sizeof(unsigned long) +
	MAX_MODULE_INPUT_TEXT_LEN + 1)];
static size_t captured_size;
static unsigned writes, allocations;
static int measuring;
static int fragmented, failed_write;
static unsigned interruptions;
void *__real_fxmalloc(size_t);
ssize_t __real_write(int, const void *, size_t);

void *__wrap_fxmalloc(size_t count)
{
	if (measuring)
	{
		allocations++;
	}
	return __real_fxmalloc(count);
}

ssize_t __wrap_write(int fd, const void *buffer, size_t count)
{
	if (fd != FIXTURE_FD)
	{
		return __real_write(fd, buffer, count);
	}
	writes++;
	if (fragmented && ((interruptions == 0 && captured_size == 0) ||
		(interruptions == 1 && captured_size == 9)))
	{
		interruptions++;
		errno = EINTR;
		return -1;
	}
	if (failed_write && (!fragmented || captured_size >= 6))
	{
		errno = failed_write;
		return failed_write == ENOSPC ? 0 : -1;
	}
	if (fragmented && count > 3)
	{
		count = 3;
	}
	assert(count <= sizeof(captured) - captured_size);
	memcpy(captured + captured_size, buffer, count);
	captured_size += count;
	return count;
}

static void test_wire_and_capacity(void)
{
	const size_t sizes[] = {0, 1, 7, 8, 9, 127,
		MAX_MODULE_INPUT_TEXT_LEN - 1, MAX_MODULE_INPUT_TEXT_LEN};
	char text[MAX_MODULE_INPUT_TEXT_LEN + 2];
	int descriptors[] = {FIXTURE_FD, -1};
	unsigned long window = ~0UL;

	memset(text, 'x', sizeof(text));
	for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	{
		unsigned long saved_window, length, continuation;
		char saved = text[sizes[i]];

		text[sizes[i]] = 0;
		writes = allocations = captured_size = 0;
		measuring = 1;
		SendText(descriptors, text, window);
		measuring = 0;
		assert(writes == 1 && allocations == 0);
		assert(captured_size == 3 * sizeof(unsigned long) + sizes[i]);
		memcpy(&saved_window, captured, sizeof(saved_window));
		memcpy(&length, captured + sizeof(window), sizeof(length));
		assert(saved_window == window && length == sizes[i]);
		assert(memcmp(captured + 2 * sizeof(window), text,
			length) == 0);
		memcpy(&continuation, captured + 2 * sizeof(window) + length,
			sizeof(continuation));
		assert(continuation == 1);
		text[sizes[i]] = saved;
	}
	text[MAX_MODULE_INPUT_TEXT_LEN + 1] = 0;
	writes = 0;
	SendText(descriptors, text, window);
	SendText(descriptors, NULL, window);
	assert(writes == 0);
}

static void test_real_pipe(void)
{
	int endpoints[2];
	unsigned char received[3 * sizeof(unsigned long) + 3];
	unsigned long header[2], continuation;

	assert(pipe(endpoints) == 0);
	int descriptors[] = {endpoints[1], endpoints[0]};

	SendText(descriptors, "Nop", 17);
	assert(read(endpoints[0], received, sizeof(received)) ==
		sizeof(received));
	memcpy(header, received, sizeof(header));
	memcpy(&continuation, received + sizeof(header) + 3,
		sizeof(continuation));
	assert(header[0] == 17 && header[1] == 3 && continuation == 1);
	assert(memcmp(received + sizeof(header), "Nop", 3) == 0);
	close(endpoints[0]);
	close(endpoints[1]);
}

static size_t check_frame(size_t cursor, const char *message)
{
	unsigned long window, length, continuation;

	assert(cursor <= captured_size);
	assert(captured_size - cursor >= 3 * sizeof(unsigned long));
	memcpy(&window, captured + cursor, sizeof(window));
	memcpy(&length, captured + cursor + sizeof(window), sizeof(length));
	assert(window == 29 && length == strlen(message));
	assert(length <= captured_size - cursor - 3 * sizeof(unsigned long));
	assert(memcmp(captured + cursor + 2 * sizeof(window), message,
		length) == 0);
	memcpy(&continuation, captured + cursor + 2 * sizeof(window) + length,
		sizeof(continuation));
	assert(continuation == 1);
	return cursor + 3 * sizeof(window) + length;
}

static void test_literal_comma_views(void)
{
	int descriptors[] = {FIXTURE_FD, -1};
	const char *expected[] = {"", "Raise", "", "Echo 'a", "b'", ""};
	size_t cursor = 0;

	writes = allocations = captured_size = 0;
	measuring = 1;
	SendFvwmPipe(descriptors, ",Raise,,Echo 'a,b',", 29);
	SendFvwmPipe(descriptors, NULL, 29);
	measuring = 0;
	assert(writes == 6 && allocations == 0);
	for (unsigned i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
	{
		cursor = check_frame(cursor, expected[i]);
	}
	assert(cursor == captured_size);
	char many[256];

	memset(many, ',', sizeof(many) - 1);
	many[sizeof(many) - 1] = 0;
	writes = allocations = captured_size = 0;
	measuring = 1;
	SendFvwmPipe(descriptors, many, 29);
	measuring = 0;
	assert(writes == 256 && allocations == 0);
	cursor = 0;
	for (unsigned i = 0; i < 256; i++)
	{
		cursor = check_frame(cursor, "");
	}
	assert(cursor == captured_size);
	char overlong[MAX_MODULE_INPUT_TEXT_LEN + 7];

	memset(overlong, 'x', MAX_MODULE_INPUT_TEXT_LEN + 1);
	memcpy(overlong + MAX_MODULE_INPUT_TEXT_LEN + 1, ",Nop", 5);
	writes = allocations = captured_size = 0;
	measuring = 1;
	SendFvwmPipe(descriptors, overlong, 29);
	measuring = 0;
	assert(writes == 1 && allocations == 0);
	assert(check_frame(0, "Nop") == captured_size && !isTerminated);
	writes = captured_size = interruptions = 0;
	fragmented = 1;
	failed_write = EIO;
	SendFvwmPipe(descriptors, "Nop,Raise,Lower", 29);
	assert(isTerminated && captured_size == 6);
	fragmented = failed_write = 0;
	isTerminated = 0;
}

static void test_partial_progress_and_failure(void)
{
	int descriptors[] = {FIXTURE_FD, -1};
	unsigned char expected[3 * sizeof(unsigned long) + 3];
	size_t expected_size;

	captured_size = 0;
	SendText(descriptors, "Nop", 19);
	expected_size = captured_size;
	memcpy(expected, captured, expected_size);
	captured_size = writes = interruptions = 0;
	fragmented = 1;
	SendText(descriptors, "Nop", 19);
	assert(!isTerminated && interruptions == 2 && writes > 2);
	assert(captured_size == expected_size);
	assert(memcmp(captured, expected, expected_size) == 0);
	for (unsigned i = 0; i < 3; i++)
	{
		captured_size = writes = interruptions = 0;
		fragmented = i == 0;
		failed_write = i == 0 ? EIO : i == 1 ? EAGAIN : ENOSPC;
		SendText(descriptors, "Nop", 19);
		assert(isTerminated);
		unsigned before = writes;

		SendText(descriptors, "Raise", 19);
		assert(writes == before && captured_size == (i == 0 ? 6 : 0));
		isTerminated = 0;
	}
	fragmented = failed_write = 0;
	puts("partial writes and interruptions preserve exact frames; "
		"failure stops output");
}

static void test_blocked_write_termination(void)
{
	int endpoints[2], ready[2], status;
	char bytes[4096] = {0};

	assert(pipe(endpoints) == 0 && pipe(ready) == 0);
	int flags = fcntl(endpoints[1], F_GETFL);

	assert(flags >= 0 && fcntl(endpoints[1], F_SETFL,
		flags | O_NONBLOCK) == 0);
	while (__real_write(endpoints[1], bytes, sizeof(bytes)) > 0)
	{
	}
	/* Fill any sub-page tail, including platforms with small pipes. */
	while (__real_write(endpoints[1], bytes, 1) > 0)
	{
	}
	assert(errno == EAGAIN || errno == EWOULDBLOCK);
	assert(fcntl(endpoints[1], F_SETFL, flags) == 0);
	pid_t child = fork();

	assert(child >= 0);
	if (child == 0)
	{
		struct sigaction action = {0};
		int descriptors[] = {endpoints[1], endpoints[0]};

		action.sa_handler = fvwmSetTerminate;
		sigemptyset(&action.sa_mask);
		assert(sigaction(SIGTERM, &action, NULL) == 0);
		close(ready[0]);
		assert(__real_write(ready[1], "R", 1) == 1);
		SendText(descriptors, "Nop", 0);
		assert(isTerminated);
		/* The sender never owns or closes the pipe descriptor. */
		assert(fcntl(endpoints[1], F_GETFD) >= 0);
		_exit(0);
	}
	close(ready[1]);
	assert(read(ready[0], bytes, 1) == 1 && bytes[0] == 'R');
	struct timespec delay = {0, 20000000};

	nanosleep(&delay, NULL);
	assert(waitpid(child, &status, WNOHANG) == 0);
	assert(kill(child, SIGTERM) == 0);
	pid_t exited = 0;

	for (unsigned attempt = 0; attempt < 200 && exited == 0; attempt++)
	{
		delay.tv_nsec = 10000000;
		exited = waitpid(child, &status, WNOHANG);
		assert(exited >= 0);
		if (exited == 0)
		{
			nanosleep(&delay, NULL);
		}
	}
	if (exited == 0)
	{
		kill(child, SIGKILL);
		waitpid(child, &status, 0);
	}
	close(endpoints[0]);
	close(endpoints[1]);
	close(ready[0]);
	assert(exited == child && WIFEXITED(status) &&
		WEXITSTATUS(status) == 0);
	puts("owned full-pipe command write cancels on termination");
}

int main(void)
{
	test_wire_and_capacity();
	test_real_pipe();
	test_literal_comma_views();
	test_partial_progress_and_failure();
	test_blocked_write_termination();
	puts("native command wire/capacity/owned-pipe checks pass "
		"without heap growth");
	return 0;
}
