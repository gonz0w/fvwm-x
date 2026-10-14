/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/module_list.h"
#include "fvwm/event_loop_policy.h"

static int probe_fd = -1;
static unsigned attempts;
static size_t accepted;
static unsigned char *expected;
static size_t expected_size;
static unsigned mode;

ssize_t __real_write(int fd, const void *data, size_t size);
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
	if (fd != probe_fd)
	{
		return __real_write(fd, data, size);
	}
	attempts++;
	if (mode == 5)
	{
		return 0;
	}
	if (mode == 6)
	{
		errno = EPIPE;
		return -1;
	}
	if (mode == 2 || mode == 3)
	{
		errno = mode == 2 ? EINTR : EAGAIN;
		return -1;
	}
	if (mode == 1 && size > 7)
	{
		size = 7;
	}
	if (mode == 4)
	{
		ssize_t result = __real_write(fd, data, size);
		if (result <= 0)
		{
			return result;
		}
		size = (size_t)result;
	}
	assert(accepted <= expected_size && size <= expected_size - accepted);
	assert(memcmp(data, expected + accepted, size) == 0);
	accepted += size;
	return (ssize_t)size;
}

static fmodule *prepare(unsigned packets, unsigned words, int endpoints[2])
{
	fmodule *module = module_create();
	unsigned long packet[FvwmPacketMaxSize] = {
		START_FLAG, M_NEW_PAGE, words, 0
	};

	assert(pipe(endpoints) == 0);
	MOD_WRITEFD(module) = probe_fd = endpoints[1];
	expected_size = (size_t)packets * words * sizeof(*packet);
	expected = malloc(expected_size);
	assert(expected);
	attempts = 0;
	accepted = 0;
	for (unsigned i = 0; i < packets; i++)
	{
		if (words > FvwmPacketHeaderSize)
		{
			packet[FvwmPacketHeaderSize] = i;
		}
		memcpy(expected + (size_t)i * words * sizeof(*packet), packet,
			words * sizeof(*packet));
		PositiveWrite(module, packet, words * sizeof(*packet));
	}
	assert(fqueue_get_length(&MOD_PIPEQUEUE(module)) == packets);
	return module;
}

static void finish(fmodule *module, int read_fd)
{
	mode = 0;
	unsigned passes = 0;
	while (!FQUEUE_IS_EMPTY(&MOD_PIPEQUEUE(module)))
	{
		unsigned before = attempts;
		size_t bytes_before = accepted;
		unsigned packets_before = fqueue_get_length(&MOD_PIPEQUEUE(module));

		assert(passes++ < 100);
		FlushMessageQueue(module);
		assert(attempts - before <= FVWM_MODULE_OUTPUT_ATTEMPT_BUDGET);
		assert(accepted - bytes_before <= FVWM_MODULE_OUTPUT_BYTE_BUDGET);
		assert(packets_before - fqueue_get_length(&MOD_PIPEQUEUE(module)) <=
			FVWM_MODULE_OUTPUT_PACKET_BUDGET);
	}
	assert(accepted == expected_size && MOD_PIPEQUEUE_BYTES(module) == 0);
	module_destroy(module);
	close(read_fd);
	probe_fd = -1;
	free(expected);
}

static void test_disconnected_reader(unsigned selected_mode)
{
	int endpoints[2];
	mode = selected_mode;
	fmodule *module = prepare(100, FvwmPacketMaxSize, endpoints);
	module_register(module);
	module->xinputRefs = 1;
	size_t queued = MOD_PIPEQUEUE_BYTES(module);
	FlushMessageQueue(module);
	assert(attempts == 1 && accepted == 0);
	module_cleanup();
	/* Existing death-row pins retain the queue until its owner is free. */
	assert(MOD_PIPEQUEUE_BYTES(module) == queued);
	module->xinputRefs = 0;
	module_cleanup();
	assert(fcntl(endpoints[1], F_GETFD) < 0 && errno == EBADF);
	close(endpoints[0]);
	probe_fd = -1;
	free(expected);
}

static void test_real_reader(Bool delayed)
{
	int endpoints[2];
	sigset_t blocked, previous;
	/* Keep this fixture's reader for its own waitpid: the normal core
	 * handler otherwise reaps any child as soon as it exits. */
	assert(sigemptyset(&blocked) == 0);
	assert(sigaddset(&blocked, SIGCHLD) == 0);
	assert(sigprocmask(SIG_BLOCK, &blocked, &previous) == 0);
	mode = 4;
	fmodule *module = prepare(1000, FvwmPacketMaxSize, endpoints);
	int flags = fcntl(endpoints[1], F_GETFL);
	assert(flags >= 0 && fcntl(endpoints[1], F_SETFL, flags | O_NONBLOCK) == 0);
	pid_t reader = fork();
	assert(reader >= 0);
	if (reader == 0)
	{
		unsigned char buffer[4096];
		size_t offset = 0;
		close(endpoints[1]);
		if (delayed)
		{
			poll(NULL, 0, 100);
		}
		for (;;)
		{
			ssize_t count = read(endpoints[0], buffer, sizeof(buffer));
			if (count < 0 && errno == EINTR)
			{
				continue;
			}
			assert(count >= 0);
			if (count == 0)
			{
				break;
			}
			assert(offset <= expected_size);
			assert((size_t)count <= expected_size - offset);
			assert(memcmp(buffer, expected + offset, (size_t)count) == 0);
			offset += (size_t)count;
		}
		assert(offset == expected_size);
		_exit(0);
	}
	unsigned passes = 0;
	while (!FQUEUE_IS_EMPTY(&MOD_PIPEQUEUE(module)))
	{
		unsigned before = attempts;
		size_t bytes_before = accepted;
		unsigned packets_before = fqueue_get_length(&MOD_PIPEQUEUE(module));
		assert(passes++ < 5000);
		FlushMessageQueue(module);
		assert(attempts - before <= FVWM_MODULE_OUTPUT_ATTEMPT_BUDGET);
		assert(accepted - bytes_before <= FVWM_MODULE_OUTPUT_BYTE_BUDGET);
		assert(packets_before - fqueue_get_length(&MOD_PIPEQUEUE(module)) <=
			FVWM_MODULE_OUTPUT_PACKET_BUDGET);
		if (!FQUEUE_IS_EMPTY(&MOD_PIPEQUEUE(module)))
		{
			struct pollfd ready = { endpoints[1], POLLOUT, 0 };
			assert(poll(&ready, 1, 1000) == 1);
		}
	}
	assert(passes > 1 && accepted == expected_size);
	assert(MOD_PIPEQUEUE_BYTES(module) == 0);
	module_destroy(module);
	close(endpoints[0]);
	int status;
	assert(waitpid(reader, &status, 0) == reader);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	assert(sigprocmask(SIG_SETMASK, &previous, NULL) == 0);
	probe_fd = -1;
	free(expected);
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	int endpoints[2];
	fmodule *module;

	mode = 0;
	module = prepare(1000, FvwmPacketMaxSize, endpoints);
	FlushMessageQueue(module);
	assert(attempts == FVWM_MODULE_OUTPUT_PACKET_BUDGET);
	assert(accepted == FVWM_MODULE_OUTPUT_PACKET_BUDGET *
		FvwmPacketMaxSize * sizeof(unsigned long));
	assert(fqueue_get_length(&MOD_PIPEQUEUE(module)) == 968);
	finish(module, endpoints[0]);
	puts("bounded-full-packet-drain: passed");

	module = prepare(100, FvwmPacketHeaderSize, endpoints);
	FlushMessageQueue(module);
	assert(attempts == FVWM_MODULE_OUTPUT_PACKET_BUDGET);
	assert(fqueue_get_length(&MOD_PIPEQUEUE(module)) == 68);
	finish(module, endpoints[0]);
	puts("small-packet-count-limit: passed");

	mode = 1;
	module = prepare(100, FvwmPacketMaxSize, endpoints);
	size_t queued_bytes = MOD_PIPEQUEUE_BYTES(module);
	FlushMessageQueue(module);
	assert(attempts == FVWM_MODULE_OUTPUT_ATTEMPT_BUDGET);
	assert(accepted == 7 * FVWM_MODULE_OUTPUT_ATTEMPT_BUDGET);
	assert(fqueue_get_length(&MOD_PIPEQUEUE(module)) == 100);
	assert(MOD_PIPEQUEUE_BYTES(module) == queued_bytes);
	finish(module, endpoints[0]);
	puts("partial-packet-offset-accounting: passed");

	for (mode = 2; mode <= 3; mode++)
	{
		unsigned selected_mode = mode;
		module = prepare(100, FvwmPacketMaxSize, endpoints);
		queued_bytes = MOD_PIPEQUEUE_BYTES(module);
		FlushMessageQueue(module);
		assert(attempts == (selected_mode == 2 ?
			FVWM_MODULE_OUTPUT_ATTEMPT_BUDGET : 1));
		assert(accepted == 0 && MOD_PIPEQUEUE_BYTES(module) == queued_bytes);
		finish(module, endpoints[0]);
		mode = selected_mode;
	}
	puts("interrupted-and-blocked-continuation: passed");

	/* The legacy synchronous branch must flush through the same queue
	 * before accepting its real module-framed unlock response. */
	int input[2];
	unsigned long words[2] = { 0, sizeof(ModuleUnlockResponse) - 1 };
	unsigned long continuation = 1;
	mode = 0;
	module = prepare(1000, FvwmPacketMaxSize, endpoints);
	assert(pipe(input) == 0);
	MOD_READFD(module) = input[0];
	assert(__real_write(input[1], words, sizeof(words)) == sizeof(words));
	assert(__real_write(input[1], ModuleUnlockResponse,
		sizeof(ModuleUnlockResponse) - 1) == sizeof(ModuleUnlockResponse) - 1);
	assert(__real_write(input[1], &continuation, sizeof(continuation)) ==
		sizeof(continuation));
	module->xSyncMask.m1 = M_NEW_PAGE;
	unsigned long final_packet[FvwmPacketMaxSize] = {
		START_FLAG, M_NEW_PAGE, FvwmPacketMaxSize, 0, 1000
	};
	expected = realloc(expected, expected_size + sizeof(final_packet));
	assert(expected);
	memcpy(expected + expected_size, final_packet, sizeof(final_packet));
	expected_size += sizeof(final_packet);
	PositiveWrite(module, final_packet, sizeof(final_packet));
	assert(attempts == 1001 && accepted == expected_size);
	assert(FQUEUE_IS_EMPTY(&MOD_PIPEQUEUE(module)));
	finish(module, endpoints[0]);
	close(input[1]);
	puts("synchronous-completion-and-unlock: passed");
	test_real_reader(False);
	test_real_reader(True);
	puts("fast-and-delayed-real-pipe-readers: passed");
	test_disconnected_reader(5);
	test_disconnected_reader(6);
	puts("zero-and-terminal-error-death-row: passed");
	fflush(stdout);
	_exit(0);
}
