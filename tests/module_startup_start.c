/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/events.h"
#include "fvwm/externs.h"
#include "fvwm/execcontext.h"
#include "fvwm/screen.h"
#include "fvwm/functions.h"
#include "fvwm/module_interface.h"
#include "fvwm/module_list.h"
#include "libs/FEvent.h"
#include "libs/fvwmsignal.h"

extern int moduleTimeout;
extern volatile sig_atomic_t isTerminated;

typedef enum
{
	TIMELY, EXPECT, IDLE, ACTIVITY, READ_PARTIAL, READ_INTERRUPT,
	DISCONNECT, READ_ERROR,
	FRAME_BOUNDARY, FINAL_BOUNDARY, SELECT_INTERRUPT, SELECT_ERROR,
	OUTPUT_PARTIAL, OUTPUT_INTERRUPT, OUTPUT_BUDGET, RETIRE, FINAL,
	NESTED, TERMINATE, CLOCK_INVALID, CLOCK_REGRESSION, BUFFERED_ESCAPE,
	REAL_ACTIVITY, REAL_ESCAPE
} probe_mode;

static probe_mode mode;
static Bool probing, inside_nested, draining;
static uint64_t now_ns;
static fmodule *producer;
static int sender_fd = -1, receiver_fd = -1;
static unsigned waits, reads, writes, commands, registrations;
static size_t written;
static pid_t child_pid;
static const exec_context_t *command_context;

uint64_t __real_events_monotonic_ns(void);
uint64_t __wrap_events_monotonic_ns(void)
{
	if (!probing || mode == REAL_ACTIVITY || mode == REAL_ESCAPE)
	{
		return __real_events_monotonic_ns();
	}
	/* Clock queries may affect errno. The readiness owner must save the
	 * select failure before calling another helper. */
	if (mode == SELECT_INTERRUPT || mode == SELECT_ERROR)
	{
		errno = EIO;
	}
	return now_ns;
}

pid_t __real_fork(void);
pid_t __wrap_fork(void)
{
	if (probing && mode != REAL_ACTIVITY && mode != REAL_ESCAPE)
	{
		return 123; /* Controlled parent path; no child process was made. */
	}
	pid_t result = __real_fork();
	if (probing && result > 0)
	{
		child_pid = result;
	}
	return result;
}

ssize_t __real_write(int fd, const void *data, size_t size);
static void send_command(const char *command, unsigned long continuation)
{
	unsigned char bytes[128];
	unsigned long header[2] = { None, strlen(command) };
	size_t size = sizeof(header) + header[1] + sizeof(continuation);
	assert(size <= sizeof(bytes));
	memcpy(bytes, header, sizeof(header));
	memcpy(bytes + sizeof(header), command, header[1]);
	memcpy(bytes + sizeof(header) + header[1], &continuation,
		sizeof(continuation));
	assert(__real_write(sender_fd, bytes, size) == (ssize_t)size);
}

void __real_module_register(fmodule *module);
void __wrap_module_register(fmodule *module)
{
	__real_module_register(module);
	if (!probing)
	{
		return;
	}
	registrations++;
	producer = module;
	if (mode != RETIRE && mode != FINAL)
	{
		module->xinputRefs++; /* Test observer, independent of startup pin. */
	}
	if (mode == REAL_ACTIVITY || mode == REAL_ESCAPE)
	{
		return;
	}
	int input[2], output[2];
	assert(pipe(input) == 0 && pipe(output) == 0);
	close(MOD_READFD(module));
	close(MOD_WRITEFD(module));
	MOD_READFD(module) = input[0];
	MOD_WRITEFD(module) = output[1];
	sender_fd = input[1];
	receiver_fd = output[0];
	assert(fcntl(input[0], F_SETFL, O_NONBLOCK) == 0);
	assert(fcntl(output[1], F_SETFL, O_NONBLOCK) == 0);
	if (inside_nested)
	{
		return;
	}
	if (mode == TIMELY)
	{
		send_command("NOP FINISHED STARTUP suffix", 1);
	}
	else if (mode == EXPECT)
	{
		send_command("NOP", 1);
		send_command("READY suffix", 1);
	}
	else if (mode == ACTIVITY)
	{
		for (unsigned i = 0; i < 12; i++)
		{
			send_command(i == 0 ? "ModuleTimeout 7" : "NOP", 1);
		}
	}
	else if (mode == READ_PARTIAL || mode == READ_INTERRUPT ||
		 mode == FRAME_BOUNDARY || mode == FINAL_BOUNDARY ||
		 mode == RETIRE || mode == FINAL || mode == NESTED)
	{
		send_command(mode == READ_PARTIAL || mode == READ_INTERRUPT ||
			mode == FRAME_BOUNDARY || mode == FINAL_BOUNDARY ?
			"ModuleTimeout 7" : "NOP",
			mode == FINAL || mode == FINAL_BOUNDARY ?
			0 : 1);
	}
	else if (mode == DISCONNECT)
	{
		close(sender_fd);
		sender_fd = -1;
	}
	else if (mode == OUTPUT_PARTIAL || mode == OUTPUT_INTERRUPT ||
		 mode == OUTPUT_BUDGET)
	{
		unsigned long packet[] = { START_FLAG, M_NEW_PAGE, 4, 0 };
		for (unsigned i = 0; i < 100; i++)
		{
			PositiveWrite(module, packet, sizeof(packet));
		}
	}
}

ssize_t __real_read(int fd, void *data, size_t size);
ssize_t __wrap_read(int fd, void *data, size_t size)
{
	if (probing && producer && fd == MOD_READFD(producer) && !draining)
	{
		reads++;
		if (mode == READ_ERROR)
		{
			errno = EIO;
			return -1;
		}
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
		if ((mode == FRAME_BOUNDARY || mode == FINAL_BOUNDARY) &&
		    reads == 2)
		{
			now_ns += UINT64_C(1000000000);
		}
	}
	return __real_read(fd, data, size);
}

ssize_t __wrap_write(int fd, const void *data, size_t size)
{
	if (probing && producer && fd == MOD_WRITEFD(producer) &&
	    (mode == OUTPUT_PARTIAL || mode == OUTPUT_INTERRUPT ||
	     mode == OUTPUT_BUDGET))
	{
		writes++;
		if (mode == OUTPUT_INTERRUPT)
		{
			now_ns += UINT64_C(250000000);
			errno = EINTR;
			return -1;
		}
		if (mode == OUTPUT_PARTIAL)
		{
			now_ns += UINT64_C(250000000);
			size = 1;
		}
		written += size;
		return (ssize_t)size;
	}
	return __real_write(fd, data, size);
}

int __real_fvwmSelect(int n, fd_set *r, fd_set *w, fd_set *e,
	struct timeval *timeout);
int __wrap_fvwmSelect(int n, fd_set *r, fd_set *w, fd_set *e,
	struct timeval *timeout)
{
	if (!probing || mode == REAL_ACTIVITY || mode == REAL_ESCAPE)
	{
		return __real_fvwmSelect(n, r, w, e, timeout);
	}
	waits++;
	assert(timeout && timeout->tv_sec == 0 && timeout->tv_usec > 0 &&
		timeout->tv_usec <= 100000);
	assert(r && FD_ISSET(MOD_READFD(producer), r));
	assert(FD_ISSET(ConnectionNumber(dpy), r));
	assert(n > MOD_READFD(producer) && n > ConnectionNumber(dpy));
	if (mode == OUTPUT_BUDGET && waits > 1)
	{
		now_ns = UINT64_C(11000000000);
	}
	else if (mode == CLOCK_INVALID)
	{
		now_ns = UINT64_MAX;
	}
	else if (mode == CLOCK_REGRESSION)
	{
		now_ns--;
	}
	else if (mode == TERMINATE)
	{
		isTerminated = 1;
	}
	else if (mode == SELECT_ERROR)
	{
		errno = EBADF;
		return -1;
	}
	if (mode == SELECT_INTERRUPT)
	{
		now_ns += (uint64_t)timeout->tv_usec * 1000;
		errno = EINTR;
		return -1;
	}
	if (inside_nested || mode == IDLE || mode == BUFFERED_ESCAPE ||
	    mode == OUTPUT_PARTIAL || mode == OUTPUT_INTERRUPT ||
	    mode == OUTPUT_BUDGET)
	{
		now_ns += (uint64_t)timeout->tv_usec * 1000;
		FD_ZERO(r);
		if (inside_nested || mode == IDLE || mode == BUFFERED_ESCAPE)
		{
			FD_ZERO(w);
			return 0;
		}
		return 1;
	}
	if (mode == ACTIVITY)
	{
		now_ns += UINT64_C(100000000);
	}
	FD_CLR(ConnectionNumber(dpy), r);
	FD_ZERO(w);
	return 1;
}

void __real_module_input_execute(fmodule_input *input);
void __wrap_module_input_execute(fmodule_input *input)
{
	if (!probing)
	{
		__real_module_input_execute(input);
		return;
	}
	commands++;
	if (mode == RETIRE || mode == FINAL)
	{
		fmodule *module = input->module;
		assert(mode == RETIRE || MOD_IS_RETIRED(module));
		module_kill(module);
		module_input_discard(input);
		module_cleanup();
		assert(module->xinputRefs == 1);
		assert(fcntl(MOD_READFD(module), F_GETFD) >= 0);
	}
	else if (mode == NESTED && !draining)
	{
		fmodule *outer = producer;
		int outer_sender = sender_fd, outer_receiver = receiver_fd;
		module_input_discard(input);
		inside_nested = True;
		execute_function_override_window(NULL, command_context,
			"ModuleSynchronous Timeout 2 /usr/bin/true", NULL, 0, NULL);
		assert(!MOD_IS_RETIRED(producer));
		assert(producer->xinputRefs == 1);
		module_kill(producer);
		producer->xinputRefs--;
		module_cleanup();
		close(sender_fd);
		close(receiver_fd);
		producer = outer;
		sender_fd = outer_sender;
		receiver_fd = outer_receiver;
		inside_nested = False;
	}
	else
	{
		__real_module_input_execute(input);
	}
}

static void finish(Bool observer)
{
	int input_fd = MOD_READFD(producer);
	module_kill(producer);
	assert(producer->xinputRefs == (observer ? 1 : 0));
	if (observer)
	{
		producer->xinputRefs--;
	}
	module_cleanup();
	assert(fcntl(input_fd, F_GETFD) == -1 && errno == EBADF);
	if (sender_fd >= 0)
	{
		close(sender_fd);
	}
	if (receiver_fd >= 0)
	{
		close(receiver_fd);
	}
	producer = NULL;
	sender_fd = receiver_fd = -1;
}

static void test_controlled(probe_mode selected, char *command,
	int seconds, const char *label)
{
	mode = selected;
	now_ns = UINT64_C(10000000000);
	waits = reads = writes = commands = registrations = 0;
	written = 0;
	moduleTimeout = seconds;
	if (mode == BUFFERED_ESCAPE)
	{
		XEvent event = { 0 };
		event.xkey.type = KeyPress;
		event.xkey.display = dpy;
		event.xkey.window = Scr.Root;
		event.xkey.root = Scr.Root;
		event.xkey.keycode = XKeysymToKeycode(dpy, XK_Escape);
		event.xkey.same_screen = True;
		FPutBackEvent(dpy, &event);
	}
	execute_function_override_window(NULL, command_context, command,
		NULL, 0, NULL);
	assert(producer != NULL);
	Bool observer = mode != RETIRE && mode != FINAL;
	assert(producer->xinputRefs == (observer ? 1 : 0));
	assert(MOD_IS_RETIRED(producer) == (mode == RETIRE || mode == FINAL ||
		mode == DISCONNECT || mode == READ_ERROR));
	assert(waits <= 300 && reads <= 24 && writes <= 32);
	if (mode == TIMELY || mode == EXPECT)
	{
		assert(now_ns == UINT64_C(10000000000));
		assert(commands == (mode == EXPECT ? 1 : 0));
	}
	if (mode == IDLE || mode == SELECT_INTERRUPT)
	{
		int expected = strstr(command, "Timeout 2") ? 2 : seconds;
		assert(now_ns == UINT64_C(10000000000) +
			(uint64_t)expected * UINT64_C(1000000000));
		assert(waits == (unsigned)expected * 10);
	}
	if (mode == ACTIVITY)
	{
		assert(commands == 9 && waits == 10 && moduleTimeout == 7);
	}
	if (mode == READ_PARTIAL || mode == READ_INTERRUPT ||
	    mode == FRAME_BOUNDARY || mode == FINAL_BOUNDARY)
	{
		assert(commands == 0);
		assert(mode != READ_PARTIAL || producer->xinputBytes == 4);
		draining = True;
		fmodule_input *input;
		assert(module_receive_queued(producer, &input) ==
			FMODULE_INPUT_READY);
		assert(strcmp(input->command, "ModuleTimeout 7") == 0);
		/* Admission, queue ownership and producer retirement after the
		 * barrier use the ordinary asynchronous path. */
		module_input_enqueue(input);
		assert(producer->xinputRefs == 2);
		ExecuteCommandQueue();
		assert(moduleTimeout == 7 && producer->xqueuedInputs == 0);
		assert(MOD_IS_RETIRED(producer) == (mode == FINAL_BOUNDARY));
		draining = False;
	}
	if (mode == SELECT_ERROR)
	{
		assert(waits == 1);
	}
	if (mode == OUTPUT_PARTIAL)
	{
		assert(written == 4 && MOD_PIPEQUEUE_BYTES(producer) > 0);
	}
	if (mode == OUTPUT_INTERRUPT)
	{
		assert(writes == 4 && written == 0);
	}
	if (mode == OUTPUT_BUDGET)
	{
		assert(writes == 32 && waits == 2);
		assert(fqueue_get_length(&MOD_PIPEQUEUE(producer)) == 68);
	}
	if (mode == NESTED)
	{
		assert(commands == 1 && registrations == 2);
		assert(now_ns == UINT64_C(12000000000));
	}
	if (mode == RETIRE || mode == FINAL)
	{
		assert(commands == 1);
	}
	if (mode == BUFFERED_ESCAPE)
	{
		assert(waits == 1);
	}
	isTerminated = 0;
	finish(observer);
	printf("%s: passed\n", label);
}

static void test_real(Bool escape, Bool override)
{
	const char *path = getenv("FVWM_STARTUP_PRODUCER");
	assert(path != NULL);
	char command[1024];
	assert(snprintf(command, sizeof(command), "ModuleSynchronous%s %s %s",
		override ? " Timeout 1" : "", path,
		escape ? "escape" : "activity") < (int)sizeof(command));
	mode = escape ? REAL_ESCAPE : REAL_ACTIVITY;
	commands = 0;
	moduleTimeout = override ? 3 : 1;
	child_pid = -1;
	uint64_t start = __real_events_monotonic_ns();
	execute_function_override_window(NULL, command_context, command,
		NULL, 0, NULL);
	uint64_t elapsed = __real_events_monotonic_ns() - start;
	assert(child_pid > 0 && producer && !MOD_IS_RETIRED(producer));
	assert(producer->xinputRefs == 1);
	if (escape)
	{
		assert(elapsed >= UINT64_C(100000000) &&
			elapsed < UINT64_C(800000000));
	}
	else
	{
		assert(elapsed >= UINT64_C(900000000) &&
			elapsed < UINT64_C(1800000000));
		unsigned inline_commands = commands;
		assert(inline_commands >= 1 && inline_commands < 8);
		Bool ready = False;
		unsigned asynchronous_commands = 0;
		uint64_t end = start + UINT64_C(4000000000);
		while (!ready && __real_events_monotonic_ns() < end)
		{
			fmodule_input *input;
			fmodule_input_status status = module_receive_queued(
				producer, &input);
			if (status == FMODULE_INPUT_READY)
			{
				if (module_input_expect(input,
					ModuleFinishedStartupResponse))
				{
					ready = True;
					module_input_discard(input);
				}
				else
				{
					asynchronous_commands++;
					module_input_enqueue(input);
					ExecuteCommandQueue();
				}
			}
			else
			{
				assert(status == FMODULE_INPUT_INCOMPLETE);
				fd_set r;
				struct timeval timeout = { 0, 100000 };
				FD_ZERO(&r);
				FD_SET(MOD_READFD(producer), &r);
				assert(fvwmSelect(MOD_READFD(producer) + 1,
					&r, NULL, NULL, &timeout) >= 0);
			}
		}
		assert(ready && inline_commands + asynchronous_commands == 8 &&
			!MOD_IS_RETIRED(producer));
		printf("Measured startup activity deadline: %.3f seconds\n",
			elapsed / 1e9);
	}
	int status;
	assert(waitpid(child_pid, &status, 0) == child_pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	finish(True);
	printf("%s: passed\n", escape ? "x-readiness-escape-real-clock" :
		override ? "real-explicit-timeout-asynchronous-continuation" :
		"real-timeout-then-asynchronous-continuation");
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	assert(moduleTimeout == 30);
	command_context = exc;
	execute_function_override_window(NULL, exc, "Key Escape A N EscapeFunc",
		NULL, 0, NULL);
	XEvent deferred = { 0 };
	deferred.type = Expose;
	deferred.xexpose.window = Scr.Root;
	deferred.xexpose.serial = 0x1256;
	FPutBackEvent(dpy, &deferred);
	sigset_t blocked, previous;
	assert(sigemptyset(&blocked) == 0);
	assert(sigaddset(&blocked, SIGCHLD) == 0);
	assert(sigprocmask(SIG_BLOCK, &blocked, &previous) == 0);
	probing = True;
	char *normal = "ModuleSynchronous /usr/bin/true";
	test_controlled(TIMELY, normal, 1, "default-response-prefix");
	test_controlled(EXPECT, "ModuleSynchronous Expect READY /usr/bin/true", 1,
		"custom-expect-and-inline-command");
	test_controlled(IDLE, normal, 1, "omitted-timeout-captures-module-timeout");
	test_controlled(IDLE, normal, 30, "thirty-second-default-finite-waits");
	test_controlled(IDLE, "ModuleSynchronous Timeout 2 /usr/bin/true", 1,
		"explicit-positive-override");
	registrations = 0;
	execute_function_override_window(NULL, exc,
		"ModuleSynchronous Timeout 0 /usr/bin/true", NULL, 0, NULL);
	execute_function_override_window(NULL, exc,
		"ModuleSynchronous Timeout -1 /usr/bin/true", NULL, 0, NULL);
	assert(registrations == 0);
	puts("invalid-timeout-refused-before-launch: passed");
	test_controlled(ACTIVITY, normal, 1, "activity-keeps-total-deadline");
	test_controlled(READ_PARTIAL, normal, 1,
		"partial-input-resumes-after-timeout");
	test_controlled(READ_INTERRUPT, normal, 1,
		"interrupted-input-resumes-after-timeout");
	test_controlled(FRAME_BOUNDARY, normal, 1,
		"complete-frame-at-deadline-resumes");
	test_controlled(FINAL_BOUNDARY, normal, 1,
		"final-frame-at-deadline-resumes");
	test_controlled(DISCONNECT, normal, 1, "disconnection-retires-producer");
	test_controlled(READ_ERROR, normal, 1, "input-error-retires-producer");
	test_controlled(SELECT_INTERRUPT, normal, 1, "interrupted-wait-bounded");
	test_controlled(SELECT_ERROR, normal, 1, "terminal-select-error-ends-barrier");
	test_controlled(OUTPUT_PARTIAL, normal, 1, "partial-output-keeps-deadline");
	test_controlled(OUTPUT_INTERRUPT, normal, 1,
		"interrupted-output-keeps-deadline");
	test_controlled(OUTPUT_BUDGET, normal, 1,
		"startup-output-retains-pass-budget");
	test_controlled(RETIRE, normal, 1, "command-retirement-pin");
	test_controlled(FINAL, normal, 1, "final-command-retirement-pin");
	test_controlled(NESTED, normal, 1, "nested-startup-preserves-parent-deadline");
	test_controlled(TERMINATE, normal, 1, "termination-ends-barrier");
	test_controlled(CLOCK_INVALID, normal, 1, "clock-failure-keeps-module-live");
	test_controlled(CLOCK_REGRESSION, normal, 1,
		"clock-regression-keeps-module-live");
	test_controlled(BUFFERED_ESCAPE, normal, 1, "buffered-escape-ends-barrier");
	test_real(False, False);
	test_real(False, True);
	test_real(True, False);
	assert(FCheckTypedEvent(dpy, Expose, &deferred));
	assert(deferred.xexpose.serial == 0x1256);
	puts("ordinary-x-event-remains-queued: passed");
	probing = False;
	assert(sigprocmask(SIG_SETMASK, &previous, NULL) == 0);
	fflush(stdout);
	_exit(0);
}
