/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/FSelection.h"
#include "libs/FEvent.h"
#include <X11/Xatom.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int64_t
now_ms(void)
{
	struct timespec now;
	assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int
ignore_error(Display *display, XErrorEvent *error)
{
	return 0;
}

static void
reply(Display *display, XSelectionRequestEvent request, const char *mode)
{
	XEvent event = {.xselection = {.type = SelectionNotify,
	    .requestor = request.requestor, .selection = request.selection,
	    .target = request.target, .property = request.property,
	    .time = request.time}};
	char *text = calloc(FSELECTION_TEXT_MAX_BYTES + 1, 1);
	assert(text != NULL);
	memset(text, 'x', FSELECTION_TEXT_MAX_BYTES + 1);
	Atom type = request.target;
	int format = 8, length = 5;
	const unsigned char *data = (const unsigned char *)"hello";
	unsigned long words[2] = {0x41414141UL, 0x42424242UL};
	unsigned short halves[2] = {0x4141, 0x4242};
	if (strcmp(mode, "max") == 0 || strcmp(mode, "oversize") == 0)
	{
		data = (unsigned char *)text;
		length = FSELECTION_TEXT_MAX_BYTES +
		    (strcmp(mode, "oversize") == 0);
	}
	else if (strcmp(mode, "empty") == 0)
	{
		length = 0;
	}
	else if (strcmp(mode, "format32") == 0 || strcmp(mode, "incr") == 0)
	{
		format = 32;
		length = 2;
		data = (unsigned char *)words;
		if (strcmp(mode, "incr") == 0)
		{
			type = XInternAtom(display, "INCR", False);
		}
	}
	else if (strcmp(mode, "format16") == 0)
	{
		format = 16;
		length = 2;
		data = (unsigned char *)halves;
	}
	else if (strcmp(mode, "type") == 0)
	{
		type = XA_ATOM;
	}
	if (strcmp(mode, "refused") == 0)
	{
		event.xselection.property = None;
	}
	else
	{
		XChangeProperty(display, request.requestor, request.property,
		    type, format, PropModeReplace, data, length);
	}
	if (strcmp(mode, "mismatch") == 0)
	{
		XEvent wrong = event;
		wrong.xselection.requestor = DefaultRootWindow(display);
		FSendEvent(display, wrong.xselection.requestor, False,
		    0, &wrong);
		wrong = event;
		wrong.xselection.target = XA_ATOM;
		FSendEvent(display, request.requestor, False, 0, &wrong);
		wrong = event;
		wrong.xselection.property = XA_CUT_BUFFER0;
		FSendEvent(display, request.requestor, False, 0, &wrong);
		wrong = event;
		wrong.xselection.time++;
		FSendEvent(display, request.requestor, False, 0, &wrong);
	}
	FSendEvent(display, request.requestor, False, 0, &event);
	XSync(display, False);
	free(text);
}

static pid_t
start_owner(Display *parent, Atom selection, const char *mode)
{
	int ready[2];
	assert(pipe(ready) == 0);
	pid_t child = fork();
	assert(child >= 0);
	if (child == 0)
	{
		close(ready[0]);
		close(ConnectionNumber(parent));
		Display *display = XOpenDisplay(NULL);
		assert(display != NULL);
		XSetErrorHandler(ignore_error);
		Window owner = XCreateSimpleWindow(display,
		    DefaultRootWindow(display),
		    0, 0, 1, 1, 0, 0, 0);
		XSetSelectionOwner(display, selection, owner, CurrentTime);
		XSync(display, False);
		assert(write(ready[1], "r", 1) == 1);
		close(ready[1]);
		XEvent event;
		do
		{
			FNextEvent(display, &event);
		} while (event.type != SelectionRequest);
		if (strcmp(mode, "silent") == 0)
		{
			for (;;)
			{
				pause();
			}
		}
		if (strcmp(mode, "death") != 0)
		{
			if (strcmp(mode, "late") == 0)
			{
				XSelectionRequestEvent first =
				    event.xselectionrequest;
				do
				{
					FNextEvent(display, &event);
				} while (event.type != SelectionRequest);
				assert(first.requestor !=
				    event.xselectionrequest.requestor);
				reply(display, first, "valid");
			}
			reply(display, event.xselectionrequest, mode);
		}
		XCloseDisplay(display);
		_exit(0);
	}
	close(ready[1]);
	char byte;
	assert(read(ready[0], &byte, 1) == 1);
	close(ready[0]);
	return child;
}

static void
signal_interrupt(int signal_number)
{
}

static void
test_case(Display *display, Atom selection, Atom property, const char *mode,
    bool valid)
{
	pid_t owner = start_owner(display, selection, mode);
	struct itimerval timer = {.it_interval = {0, 10000},
	    .it_value = {0, 10000}};
	if (strcmp(mode, "silent") == 0)
	{
		assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
	}
	int64_t start = now_ms();
	char *text = FSelectionRequestText(display, selection, XA_STRING,
	    property,
	    CurrentTime, 200);
	int64_t elapsed = now_ms() - start;
	timer = (struct itimerval){0};
	assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
	assert((text != NULL) == valid);
	if (valid)
	{
		size_t expected = strcmp(mode, "max") == 0 ?
		    FSELECTION_TEXT_MAX_BYTES :
		    (strcmp(mode, "empty") == 0 ? 0 : 5);
		assert(strlen(text) == expected);
		if (expected == 5)
		{
			assert(strcmp(text, "hello") == 0);
		}
	}
	assert(elapsed < 1500);
	if (strcmp(mode, "silent") == 0 || strcmp(mode, "death") == 0)
	{
		assert(elapsed >= 180);
	}
	free(text);
	if (strcmp(mode, "silent") == 0)
	{
		kill(owner, SIGTERM);
	}
	int status;
	assert(waitpid(owner, &status, 0) == owner);
	if (strcmp(mode, "silent") != 0)
	{
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	}
	printf("selection %s passed (%lld ms)\n", mode, (long long)elapsed);
}

int
main(void)
{
	/* Exercise the production poll path above select's descriptor ceiling
	 * where permitted. Close the fillers afterward. */
	int fillers[FD_SETSIZE + 8];
	size_t filler_count = 0;
	struct rlimit limit;
	assert(getrlimit(RLIMIT_NOFILE, &limit) == 0);
	if (limit.rlim_max > FD_SETSIZE + 8 && limit.rlim_cur <= FD_SETSIZE + 8)
	{
		limit.rlim_cur = FD_SETSIZE + 9;
		assert(setrlimit(RLIMIT_NOFILE, &limit) == 0);
	}
	if (limit.rlim_cur > FD_SETSIZE + 8)
	{
		int fd;
		do
		{
			fd = open("/dev/null", O_RDONLY);
			assert(fd >= 0 && filler_count < FD_SETSIZE + 8);
			fillers[filler_count++] = fd;
		} while (fd < FD_SETSIZE);
	}
	Display *display = XOpenDisplay(NULL);
	assert(display != NULL);
	printf("X connection fd %d (select ceiling %d)\n",
	    ConnectionNumber(display), FD_SETSIZE);
	if (filler_count != 0)
	{
		assert(ConnectionNumber(display) > FD_SETSIZE);
	}
	for (size_t i = 0; i < filler_count; i++)
	{
		close(fillers[i]);
	}
	Atom selection = XInternAtom(display, "FVWM_SELECTION_TEST", False);
	Atom property = XInternAtom(display, "FVWM_SELECTION_DATA", False);
	XEvent expose = {.xexpose = {.type = Expose,
	    .window = DefaultRootWindow(display), .count = 123}};
	FPutBackEvent(display, &expose);
	const char *valid[] = {"valid", "empty", "max", "mismatch"};
	const char *invalid[] = {"oversize", "format32", "format16", "type",
	    "incr", "refused", "death"};
	for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
	{
		test_case(display, selection, property, valid[i], true);
	}
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		test_case(display, selection, property, invalid[i], false);
	}
	assert(FCheckTypedEvent(display, Expose, &expose));
	assert(expose.xexpose.count == 123);
	struct sigaction action = {.sa_handler = signal_interrupt};
	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGALRM, &action, NULL) == 0);
	test_case(display, selection, property, "silent", false);
	pid_t owner = start_owner(display, selection, "late");
	assert(FSelectionRequestText(display, selection, XA_STRING,
	    property,
	    CurrentTime, 100) == NULL);
	char *text = FSelectionRequestText(display, selection, XA_STRING,
	    property,
	    CurrentTime, 500);
	assert(text != NULL && strcmp(text, "hello") == 0);
	free(text);
	int status;
	assert(waitpid(owner, &status, 0) == owner);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	XCloseDisplay(display);
	puts("selection isolation, event preservation and deadline passed");
	return 0;
}
