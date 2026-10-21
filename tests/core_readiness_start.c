/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/module_records.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/events.h"
#include "fvwm/module_list.h"
#include "fvwm/session.h"

static fmodule *producer, *victim;
static Bool probing, interactive, interrupt_once;
static unsigned polls, accepted, order_count;
static char order[8];
static int retired_fd, session_write;

int __real_poll(struct pollfd *, nfds_t, int);
void __real_FlushMessageQueue(fmodule *module);
void __real_module_input_enqueue(fmodule_input *input);
void __real_ProcessICEMsgs(void);

int __wrap_poll(struct pollfd *fds, nfds_t count, int timeout)
{
	if (probing)
	{
		Bool saw_session = False, saw_producer = False;

		polls++;
		assert(producer->xinputRefs > 0);
		if (victim != NULL)
		{
			assert(victim->xinputRefs > 0);
		}
		for (nfds_t i = 0; i < count; i++)
		{
			saw_session |= sm_fd >= 0 && fds[i].fd == sm_fd;
			saw_producer |= fds[i].fd == MOD_READFD(producer);
		}
		assert(saw_producer);
		assert(saw_session == (sm_fd >= 0 && !interactive));
		if (interrupt_once)
		{
			interrupt_once = False;
			errno = EINTR;
			return -1;
		}
	}
	return __real_poll(fds, count, timeout);
}

void __wrap_module_input_enqueue(fmodule_input *input)
{
	if (!probing)
	{
		__real_module_input_enqueue(input);
		return;
	}
	assert(input->module == producer);
	assert(strcmp(input->command, "Echo readiness") == 0);
	assert(producer->xinputRefs >= 2);
	assert(order_count < sizeof(order));
	order[order_count++] = 'R';
	accepted++;
	module_input_discard(input);
}

void __wrap_FlushMessageQueue(fmodule *module)
{
	if (probing)
	{
		assert(module == producer);
		assert(order_count == 1 && order[0] == 'R');
		order[order_count++] = 'W';
		/* An earlier output callback can retire a later watched owner.
		 * Keep the record until this snapshot unwinds. */
		assert(victim != NULL && victim->xinputRefs > 0);
		module_kill(victim);
		module_cleanup();
		assert(fcntl(retired_fd, F_GETFD) >= 0);
	}
	__real_FlushMessageQueue(module);
}

void __wrap_ProcessICEMsgs(void)
{
	char byte;

	if (!probing)
	{
		__real_ProcessICEMsgs();
		return;
	}
	assert(!interactive && order_count == 2);
	assert(read(sm_fd, &byte, 1) == 1 && byte == 's');
	order[order_count++] = 'S';
}

static fmodule *make_producer(int *writer, Bool high)
{
	int fds[2];
	fmodule *module = module_create();
	struct rlimit limit;

	assert(pipe(fds) == 0);
	if (high)
	{
		assert(getrlimit(RLIMIT_NOFILE, &limit) == 0);
		if (limit.rlim_cur > FD_SETSIZE + 1)
		{
			int duplicate = fcntl(fds[0], F_DUPFD, FD_SETSIZE);

			assert(duplicate >= FD_SETSIZE);
			close(fds[0]);
			fds[0] = duplicate;
			puts("core high descriptor: real");
		}
	}
	int flags = fcntl(fds[0], F_GETFL);

	assert(flags >= 0 && fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) == 0);
	MOD_READFD(module) = fds[0];
	module_register(module);
	*writer = fds[1];
	return module;
}

static size_t make_frame(unsigned char frame[64])
{
	Window window = None;
	const char command[] = "Echo readiness";
	unsigned long length = sizeof(command) - 1, continuation = 1;
	size_t offset = 0;

	memcpy(frame + offset, &window, sizeof(window));
	offset += sizeof(window);
	memcpy(frame + offset, &length, sizeof(length));
	offset += sizeof(length);
	memcpy(frame + offset, command, length);
	offset += length;
	memcpy(frame + offset, &continuation, sizeof(continuation));
	return offset + sizeof(continuation);
}

static void turn(Bool interaction)
{
	XEvent event;

	XSync(dpy, False);
	while (XPending(dpy))
	{
		XNextEvent(dpy, &event);
	}
	interactive = interaction;
	probing = True;
	if (interaction)
	{
		assert(events_interaction_poll(0));
	}
	else
	{
		(void)My_XNextEvent(dpy, &event);
	}
	probing = False;
}

static void test_partial(Bool interaction)
{
	int writer;
	unsigned char frame[64];
	size_t size = make_frame(frame);

	producer = make_producer(&writer, True);
	victim = NULL;
	accepted = order_count = polls = 0;
	assert(write(writer, frame, sizeof(Window)) == sizeof(Window));
	turn(interaction);
	assert(polls > 0 && accepted == 0 && producer->xinputRefs == 0);
	assert(producer->xinputBytes == sizeof(Window));
	assert(write(writer, frame + sizeof(Window), size - sizeof(Window)) ==
		(ssize_t)(size - sizeof(Window)));
	turn(interaction);
	assert(accepted == 1 && producer->xinputRefs == 0);
	module_kill(producer);
	module_cleanup();
	close(writer);
	puts(interaction ? "interaction partial continuation: passed" :
		"ordinary partial continuation: passed");
}

static void test_order_and_retirement(Bool interaction)
{
	int writer, victim_writer, output[2], session[2];
	unsigned char frame[64];
	size_t size = make_frame(frame);
	unsigned long packet[] = {START_FLAG, M_NEW_PAGE, 4, 0};

	victim = make_producer(&victim_writer, False);
	retired_fd = MOD_READFD(victim);
	producer = make_producer(&writer, False);
	assert(pipe(output) == 0 && pipe(session) == 0);
	MOD_WRITEFD(producer) = output[1];
	PositiveWrite(producer, packet, sizeof(packet));
	assert(!FQUEUE_IS_EMPTY(&MOD_PIPEQUEUE(producer)));
	sm_fd = session[0];
	session_write = session[1];
	assert(write(session_write, "s", 1) == 1);
	assert(write(writer, frame, size) == (ssize_t)size);
	assert(write(victim_writer, frame, size) == (ssize_t)size);
	accepted = order_count = polls = 0;
	interrupt_once = True;
	turn(interaction);
	if (interaction)
	{
		assert(polls == 1 && accepted == 0);
		turn(interaction);
	}
	assert(polls == 2 && accepted == 1);
	assert(order_count == (interaction ? 2 : 3));
	assert(memcmp(order, interaction ? "RW" : "RWS", order_count) == 0);
	assert(producer->xinputRefs == 0);
	assert(fcntl(retired_fd, F_GETFD) == -1 && errno == EBADF);
	unsigned long received[4];

	assert(read(output[0], received, sizeof(received)) == sizeof(received));
	assert(memcmp(packet, received, sizeof(packet)) == 0);
	module_kill(producer);
	module_cleanup();
	close(writer);
	close(victim_writer);
	close(output[0]);
	close(session[0]);
	close(session_write);
	sm_fd = -1;
	victim = NULL;
	puts(interaction ? "interaction order/retirement/EINTR: passed" :
		"ordinary order/retirement/EINTR: passed");
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	fFvwmInStartup = False;
	alarm(20);
	test_partial(False);
	test_partial(True);
	test_order_and_retirement(False);
	test_order_and_retirement(True);
	puts("core readiness snapshot: passed");
	fflush(stdout);
	_exit(0);
}
