/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Module.h"
#include "libs/fvwmsignal.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define FIXTURE_FD 4242
static unsigned long wire[FvwmPacketMaxSize];
static size_t available, wire_offset;
static unsigned calls, interruptions;
static int inject_interruptions, inject_error;

ssize_t __real_read(int, void *, size_t);

ssize_t __wrap_read(int fd, void *buffer, size_t count)
{
	if (fd != FIXTURE_FD)
	{
		return __real_read(fd, buffer, count);
	}
	calls++;
	if (inject_error)
	{
		errno = EIO;
		return -1;
	}
	const size_t offsets[] = {0, sizeof(unsigned long) + 1,
		FvwmPacketHeaderSize_byte + 1};
	if (inject_interruptions && interruptions < 3 &&
	    wire_offset == offsets[interruptions])
	{
		interruptions++;
		errno = EINTR;
		return -1;
	}
	if (count > 3)
	{
		count = 3;
	}
	if (inject_interruptions && interruptions < 3 &&
	    wire_offset < offsets[interruptions] &&
	    count > offsets[interruptions] - wire_offset)
	{
		count = offsets[interruptions] - wire_offset;
	}
	if (count > available - wire_offset)
	{
		count = available - wire_offset;
	}
	memcpy(buffer, (char *)wire + wire_offset, count);
	wire_offset += count;
	return count;
}

static void prepare(unsigned long words)
{
	memset(wire, 0, sizeof(wire));
	wire[0] = START_FLAG;
	wire[1] = M_NEW_PAGE;
	wire[2] = words;
	wire[3] = 123;
	for (unsigned long i = FvwmPacketHeaderSize; i < words; i++)
	{
		wire[i] = 0x100 + i;
	}
	available = words * sizeof(*wire);
	wire_offset = calls = interruptions = 0;
	inject_interruptions = inject_error = 0;
	isTerminated = 0;
}

static void test_interrupted_and_fragmented(void)
{
	prepare(FvwmPacketHeaderSize + 2);
	inject_interruptions = 1;
	FvwmPacket *packet = ReadFvwmPacket(FIXTURE_FD);

	assert(packet != NULL && wire_offset == available && interruptions == 3);
	assert(memcmp(packet, wire, available) == 0);
	assert(ReadFvwmPacket(FIXTURE_FD) == NULL);
	puts("fragmented header/body survive three nonterminal interruptions");
}

static void test_boundaries_and_errors(void)
{
	prepare(FvwmPacketHeaderSize);
	assert(ReadFvwmPacket(FIXTURE_FD) != NULL && wire_offset == available);
	prepare(FvwmPacketMaxSize);
	FvwmPacket *packet = ReadFvwmPacket(FIXTURE_FD);

	assert(packet != NULL && memcmp(packet, wire, available) == 0);
	prepare(FvwmPacketHeaderSize);
	available -= 1;
	assert(ReadFvwmPacket(FIXTURE_FD) == NULL);
	prepare(FvwmPacketHeaderSize);
	inject_error = 1;
	assert(ReadFvwmPacket(FIXTURE_FD) == NULL && errno == EIO);
	prepare(FvwmPacketHeaderSize);
	isTerminated = 1;
	assert(ReadFvwmPacket(FIXTURE_FD) == NULL && calls == 0);
	assert(errno == EINTR);
	isTerminated = 0;
}

static void test_actual_pipe(void)
{
	int endpoints[2];

	prepare(FvwmPacketHeaderSize + 2);
	assert(pipe(endpoints) == 0);
	assert(write(endpoints[1], wire, available) == (ssize_t)available);
	FvwmPacket *packet = ReadFvwmPacket(endpoints[0]);

	assert(packet != NULL && memcmp(packet, wire, available) == 0);
	close(endpoints[0]);
	close(endpoints[1]);
}

static void test_declared_word_count(void)
{
	const unsigned long invalid[] = {0, FvwmPacketHeaderSize - 1,
		FvwmPacketMaxSize + 1, ULONG_MAX,
		FvwmPacketHeaderSize + ULONG_MAX / sizeof(unsigned long) + 1};

	for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		prepare(FvwmPacketHeaderSize);
		wire[2] = invalid[i];
		assert(ReadFvwmPacket(FIXTURE_FD) == NULL);
		assert(wire_offset == FvwmPacketHeaderSize_byte && errno == EPROTO);
	}
}

static void test_bad_start_and_truncation(void)
{
	prepare(FvwmPacketHeaderSize);
	memmove(wire + 1, wire, available);
	wire[0] = 0;
	available += sizeof(*wire);
	assert(ReadFvwmPacket(FIXTURE_FD) == NULL && errno == EPROTO);
	assert(wire_offset == sizeof(*wire));
	/* Refuse every truncated position, including exact header EOF. */
	for (size_t bytes = 1;
	    bytes < (FvwmPacketHeaderSize + 2) * sizeof(*wire); bytes++)
	{
		prepare(FvwmPacketHeaderSize + 2);
		available = bytes;
		assert(ReadFvwmPacket(FIXTURE_FD) == NULL && errno == EPROTO);
	}
	prepare(FvwmPacketHeaderSize);
	available = 0;
	errno = EIO;
	assert(ReadFvwmPacket(FIXTURE_FD) == NULL && errno == 0);
}

static void terminate(int number)
{
	fvwmSetTerminate(number);
}

static void test_blocked_termination(void)
{
	int input[2], ready[2], status = 0;
	char byte;

	assert(pipe(input) == 0 && pipe(ready) == 0);
	pid_t child = fork();

	assert(child >= 0);
	if (child == 0)
	{
		struct sigaction action;

		memset(&action, 0, sizeof(action));
		action.sa_handler = terminate;
		sigemptyset(&action.sa_mask);
		assert(sigaction(SIGTERM, &action, NULL) == 0);
		close(ready[0]);
		assert(write(ready[1], "R", 1) == 1);
		assert(fvwmRead(input[0], &byte, 1) == -1);
		assert(errno == EINTR && isTerminated);
		_exit(0);
	}
	close(ready[1]);
	assert(__real_read(ready[0], &byte, 1) == 1 && byte == 'R');
	assert(kill(child, SIGTERM) == 0);
	pid_t exited = 0;

	for (unsigned attempt = 0; attempt < 200 && exited == 0; attempt++)
	{
		struct timespec delay = {0, 10000000};

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
	close(input[0]);
	close(input[1]);
	close(ready[0]);
	assert(exited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
	puts("owned blocked read cancels on termination");
}

int main(void)
{
	test_interrupted_and_fragmented();
	test_boundaries_and_errors();
	test_actual_pipe();
	test_declared_word_count();
	test_bad_start_and_truncation();
	test_blocked_termination();
	return 0;
}
