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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/events.h"
#include "fvwm/execcontext.h"
#include "fvwm/module_list.h"
#include "libs/fvwmsignal.h"

extern int moduleTimeout;
extern volatile sig_atomic_t isTerminated;

typedef enum
{
	TIMELY, IDLE, WRITE_PARTIAL, WRITE_INTERRUPT, WRITE_BLOCKED,
	READ_INTERRUPT, READ_PARTIAL, WRITE_THEN_WAIT, DISCONNECT, SELECT_ERROR,
	ACTIVITY, NESTED, RETIRE, FINAL, TERMINATE,
	CLOCK_INVALID, CLOCK_REGRESSION, REAL_ACTIVITY
} probe_mode;

static probe_mode mode;
static uint64_t now_ns;
static int input_fd = -1;
static int output_fd = -1;
static unsigned write_calls, read_calls, wait_calls, commands;
static size_t written;
static fmodule *nested_module;
static Bool inside_nested;

uint64_t __real_events_monotonic_ns(void);
uint64_t __wrap_events_monotonic_ns(void)
{
	return mode == REAL_ACTIVITY ? __real_events_monotonic_ns() : now_ns;
}

ssize_t __real_write(int fd, const void *data, size_t size);
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
	if (fd != output_fd)
	{
		return __real_write(fd, data, size);
	}
	write_calls++;
	if (mode == WRITE_PARTIAL)
	{
		now_ns += UINT64_C(250000000);
		size = 1;
	}
	else if (mode == WRITE_INTERRUPT || mode == WRITE_BLOCKED)
	{
		now_ns += UINT64_C(100000000);
		errno = mode == WRITE_INTERRUPT ? EINTR : EAGAIN;
		return -1;
	}
	if (mode == WRITE_THEN_WAIT)
	{
		now_ns += UINT64_C(500000000);
	}
	written += size;
	return (ssize_t)size;
}

ssize_t __real_read(int fd, void *data, size_t size);
ssize_t __wrap_read(int fd, void *data, size_t size)
{
	if (fd == input_fd)
	{
		read_calls++;
		if (mode == READ_INTERRUPT)
		{
			now_ns += UINT64_C(250000000);
			errno = EINTR;
			return -1;
		}
		if (mode == READ_PARTIAL)
		{
			now_ns += UINT64_C(250000000);
			size = 1;
		}
	}
	return __real_read(fd, data, size);
}

int __real_fvwmSelect(int n, fd_set *r, fd_set *w, fd_set *e,
	struct timeval *timeout);
int __wrap_fvwmSelect(int n, fd_set *r, fd_set *w, fd_set *e,
	struct timeval *timeout)
{
	if (mode == REAL_ACTIVITY ||
	    !((r && FD_ISSET(input_fd, r)) ||
	      (w && FD_ISSET(output_fd, w))))
	{
		return __real_fvwmSelect(n, r, w, e, timeout);
	}
	wait_calls++;
	assert(timeout && timeout->tv_sec >= 0 && timeout->tv_usec >= 0);
	assert(timeout->tv_sec <= 1 && timeout->tv_usec < 1000000);
	if (mode == CLOCK_INVALID)
	{
		now_ns = UINT64_MAX;
		return 1;
	}
	if (mode == CLOCK_REGRESSION)
	{
		now_ns--;
		return 1;
	}
	if (mode == TERMINATE)
	{
		isTerminated = 1;
		errno = EINTR;
		return -1;
	}
	if (mode == SELECT_ERROR)
	{
		errno = EBADF;
		return -1;
	}
	if (mode == IDLE || mode == WRITE_BLOCKED || mode == WRITE_THEN_WAIT ||
	    (mode == NESTED && inside_nested))
	{
		now_ns += UINT64_C(250000000);
		errno = EINTR;
		return -1;
	}
	if (mode == ACTIVITY || mode == NESTED)
	{
		now_ns += UINT64_C(250000000);
	}
	return 1;
}

void __real_module_input_execute(fmodule_input *input);
void __wrap_module_input_execute(fmodule_input *input)
{
	fmodule *module = input->module;

	commands++;
	if (mode == NESTED)
	{
		unsigned long packet[] = { START_FLAG, M_NEW_PAGE, 4, 0 };

		inside_nested = True;
		PositiveWrite(nested_module, packet, sizeof(packet));
		inside_nested = False;
		assert(MOD_IS_RETIRED(nested_module));
		module_input_discard(input);
	}
	else if (mode == RETIRE || mode == FINAL)
	{
		assert(mode == RETIRE || MOD_IS_RETIRED(module));
		module_kill(module);
		module_input_discard(input);
		module_cleanup();
		/* These cases deliberately have no observer pin: only the
		 * synchronous frame protects this record after input discard. */
		assert(module->xinputRefs == 1);
		assert(fcntl(MOD_READFD(module), F_GETFD) >= 0);
	}
	else
	{
		__real_module_input_execute(input);
	}
}

static void send_command(int fd, const char *command, unsigned long cont)
{
	unsigned char bytes[128];
	unsigned long header[2] = { 0, strlen(command) };
	size_t size = sizeof(header) + header[1] + sizeof(cont);
	assert(size <= sizeof(bytes));
	memcpy(bytes, header, sizeof(header));
	memcpy(bytes + sizeof(header), command, header[1]);
	memcpy(bytes + sizeof(header) + header[1], &cont, sizeof(cont));
	assert(__real_write(fd, bytes, size) == (ssize_t)size);
}

static fmodule *prepare(int input[2], int output[2], Bool observe)
{
	fmodule *module = module_create();

	assert(pipe(input) == 0 && pipe(output) == 0);
	MOD_READFD(module) = input[0];
	MOD_WRITEFD(module) = output[1];
	assert(fcntl(input[0], F_SETFL, O_NONBLOCK) == 0);
	assert(fcntl(output[1], F_SETFL, O_NONBLOCK) == 0);
	module->xSyncMask.m1 = M_NEW_PAGE;
	module->xinputRefs = observe ? 1 : 0;
	module_register(module);
	return module;
}

static void finish(fmodule *module, int input[2], int output[2], Bool observe)
{
	int fd = MOD_READFD(module);

	module_kill(module);
	if (observe)
	{
		assert(module->xinputRefs == 1);
		module->xinputRefs--;
	}
	else
	{
		assert(module->xinputRefs == 0);
	}
	module_cleanup();
	assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
	close(input[1]);
	close(output[0]);
	input_fd = output_fd = -1;
}

static void test_controlled(probe_mode selected_mode, const char *label)
{
	int input[2], output[2], nested_input[2], nested_output[2];
	Bool observe = selected_mode != RETIRE && selected_mode != FINAL;
	fmodule *module = prepare(input, output, observe);
	unsigned long packet[] = { START_FLAG, M_NEW_PAGE, 4, 0 };

	input_fd = input[0];
	output_fd = output[1];
	mode = selected_mode;
	now_ns = UINT64_C(10000000000);
	write_calls = read_calls = wait_calls = commands = 0;
	written = 0;
	moduleTimeout = 1;
	if (mode == NESTED)
	{
		nested_module = prepare(nested_input, nested_output, True);
		/* Share wrapped readiness with the nested producer but send
		 * its bytes through its actual, empty nonblocking output pipe. */
		MOD_READFD(nested_module) = input_fd;
		close(nested_input[0]);
	}
	for (unsigned i = 0; i < 8; i++)
	{
		send_command(input[1], "NOP", mode == FINAL ? 0 : 1);
	}
	/* Timely completion gets UNLOCK first, without executing commands. */
	if (mode == TIMELY)
	{
		unsigned char discard[256];
		while (__real_read(input[0], discard, sizeof(discard)) > 0)
		{
		}
		send_command(input[1], ModuleUnlockResponse, 1);
	}
	if (mode == DISCONNECT)
	{
		unsigned char discard[256];
		while (__real_read(input[0], discard, sizeof(discard)) > 0)
		{
		}
		close(input[1]);
		input[1] = -1;
	}
	PositiveWrite(module, packet, sizeof(packet));
	assert(module->xinputRefs == (observe ? 1 : 0));
	assert(MOD_IS_RETIRED(module) == (mode != TIMELY && mode != TERMINATE));
	assert(write_calls <= 10 && read_calls <= 10 && wait_calls <= 10);
	if (mode == TIMELY)
	{
		assert(written == sizeof(packet) && commands == 0);
		assert(MOD_PIPEQUEUE_BYTES(module) == 0);
	}
	if (mode == ACTIVITY)
	{
		assert(commands == 3 && now_ns == UINT64_C(11000000000));
	}
	if (mode == WRITE_PARTIAL)
	{
		assert(written == 4 && MOD_PIPEQUEUE_BYTES(module) > 0);
	}
	if (mode == READ_PARTIAL)
	{
		assert(module->xinputBytes == 4 && commands == 0);
	}
	if (mode == WRITE_THEN_WAIT)
	{
		assert(now_ns == UINT64_C(11000000000) && wait_calls == 2);
	}
	if (mode == RETIRE || mode == FINAL || mode == NESTED)
	{
		assert(commands == 1);
	}
	if (mode == NESTED)
	{
		/* The nested frame started later and had its own second. The
		 * parent must notice its deadline expired while it was away. */
		assert(now_ns == UINT64_C(11250000000));
		MOD_READFD(nested_module) = -1;
		finish(nested_module, nested_input, nested_output, True);
	}
	isTerminated = 0;
	finish(module, input, output, observe);
	printf("%s: passed\n", label);
}

static void test_real_activity(void)
{
	int input[2], output[2], status;
	sigset_t blocked, previous;
	unsigned long packet[] = { START_FLAG, M_NEW_PAGE, 4, 0 };

	assert(sigemptyset(&blocked) == 0);
	assert(sigaddset(&blocked, SIGCHLD) == 0);
	assert(sigprocmask(SIG_BLOCK, &blocked, &previous) == 0);
	fmodule *module = prepare(input, output, True);
	input_fd = input[0];
	output_fd = output[1];
	mode = REAL_ACTIVITY;
	moduleTimeout = 1;
	pid_t sender = fork();
	assert(sender >= 0);
	if (sender == 0)
	{
		close(input[0]);
		close(output[0]);
		close(output[1]);
		for (unsigned i = 0; i < 8; i++)
		{
			send_command(input[1], "NOP", 1);
			assert(poll(NULL, 0, 250) == 0);
		}
		send_command(input[1], ModuleUnlockResponse, 1);
		_exit(0);
	}
	uint64_t start = __real_events_monotonic_ns();
	PositiveWrite(module, packet, sizeof(packet));
	uint64_t elapsed = __real_events_monotonic_ns() - start;
	assert(MOD_IS_RETIRED(module));
	assert(elapsed >= UINT64_C(900000000) &&
		elapsed < UINT64_C(1800000000));
	assert(waitpid(sender, &status, 0) == sender);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	finish(module, input, output, True);
	assert(sigprocmask(SIG_SETMASK, &previous, NULL) == 0);
	printf("activity-total-deadline-real-clock: passed\n"
		"Measured activity deadline: %.3f seconds\n", elapsed / 1e9);
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	test_controlled(TIMELY, "timely-unlock");
	test_controlled(IDLE, "idle-and-interrupted-wait");
	test_controlled(WRITE_PARTIAL, "partial-write-deadline");
	test_controlled(WRITE_INTERRUPT, "interrupted-write-deadline");
	test_controlled(WRITE_BLOCKED, "blocked-write-deadline");
	test_controlled(READ_INTERRUPT, "interrupted-read-deadline");
	test_controlled(READ_PARTIAL, "partial-read-deadline");
	test_controlled(WRITE_THEN_WAIT, "output-and-unlock-share-deadline");
	test_controlled(DISCONNECT, "disconnected-input-retires-producer");
	test_controlled(SELECT_ERROR, "readiness-error-retires-producer");
	test_controlled(ACTIVITY, "activity-does-not-refresh-deadline");
	test_controlled(NESTED, "nested-deadline-preserves-parent");
	test_controlled(RETIRE, "command-retirement-pin");
	test_controlled(FINAL, "final-command-retirement-pin");
	test_controlled(TERMINATE, "termination-cancels-wait");
	test_controlled(CLOCK_INVALID, "clock-failure-retires-producer");
	test_controlled(CLOCK_REGRESSION, "clock-regression-retires-producer");
	test_real_activity();
	fflush(stdout);
	_exit(0);
}
