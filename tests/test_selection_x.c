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
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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

/* Independent owners below exercise only the additive typed API. The legacy
 * cases above deliberately retain their original direct/INCR-refusal contract.
 */
typedef enum {
	TRANSPORT_EXACT,
	TRANSPORT_STRING,
	TRANSPORT_UTF8,
	TRANSPORT_EMPTY,
	TRANSPORT_MAX,
	TRANSPORT_OVERSIZE,
	TRANSPORT_EMBEDDED_NUL,
	TRANSPORT_TRAILING_NUL,
	TRANSPORT_BAD_UTF8,
	TRANSPORT_TARGETS_TYPE,
	TRANSPORT_TARGETS_FORMAT,
	TRANSPORT_TARGETS_LARGE,
	TRANSPORT_TARGETS_EMPTY,
	TRANSPORT_TARGETS_INCR,
	TRANSPORT_TARGETS_INCR_HINT,
	TRANSPORT_TARGETS_INCR_AGGREGATE,
	TRANSPORT_TARGETS_INCR_TYPE,
	TRANSPORT_TARGETS_INCR_TERMINAL,
	TRANSPORT_LEGACY,
	TRANSPORT_UTF8_REFUSED,
	TRANSPORT_UTF8_ONLY_REFUSED,
	TRANSPORT_REFUSED,
	TRANSPORT_WRONG_REQUESTOR,
	TRANSPORT_WRONG_SELECTION,
	TRANSPORT_WRONG_TARGET,
	TRANSPORT_WRONG_PROPERTY,
	TRANSPORT_WRONG_TIME,
	TRANSPORT_WRONG_NONE,
	TRANSPORT_INCR,
	TRANSPORT_INCR_EMPTY,
	TRANSPORT_INCR_EXACT,
	TRANSPORT_INCR_MAX,
	TRANSPORT_INCR_HEADER_ZERO,
	TRANSPORT_INCR_HEADER_TWO,
	TRANSPORT_INCR_HEADER_FORMAT,
	TRANSPORT_INCR_HEADER_LARGE,
	TRANSPORT_INCR_CHUNK_TYPE,
	TRANSPORT_INCR_CHUNK_FORMAT,
	TRANSPORT_INCR_AGGREGATE,
	TRANSPORT_INCR_TERMINAL_TYPE,
	TRANSPORT_INCR_TERMINAL_FORMAT,
	TRANSPORT_INCR_MISSING_TERMINAL,
	TRANSPORT_INCR_FLOOD,
	TRANSPORT_INCR_FOREIGN,
	TRANSPORT_INCR_REPEATED,
	TRANSPORT_INCR_DEATH,
	TRANSPORT_CANCEL,
	TRANSPORT_SERVICE,
	TRANSPORT_SERVICE_DEADLINE,
	TRANSPORT_DEADLINE,
	TRANSPORT_LATE
} transport_scenario;

typedef struct {
	Display *display;
	Window window;
	Atom auxiliary;
	Atom cancel;
	Window cancelled_requestor;
	unsigned int calls;
	unsigned int served;
	bool slow;
} transport_service_context;

static void
transport_delay(unsigned int milliseconds)
{
	struct timespec delay = {.tv_sec = milliseconds / 1000,
	    .tv_nsec = (milliseconds % 1000) * 1000000L};

	while (nanosleep(&delay, &delay) != 0)
	{
		assert(errno == EINTR);
	}
}

static bool
transport_event(Display *display, XEvent *event)
{
	int64_t deadline = now_ms() + 2000;
	struct pollfd descriptor = {ConnectionNumber(display), POLLIN, 0};

	while (now_ms() < deadline)
	{
		if (FPending(display))
		{
			FNextEvent(display, event);
			return true;
		}
		int ready = poll(&descriptor, 1, deadline - now_ms());
		assert(ready >= 0 || errno == EINTR);
	}
	return false;
}

static bool
transport_deleted(Display *display, const XSelectionRequestEvent *request)
{
	XEvent event;

	while (transport_event(display, &event))
	{
		if (event.type == DestroyNotify &&
		    event.xdestroywindow.window == request->requestor)
		{
			return false;
		}
		if (event.type == PropertyNotify &&
		    event.xproperty.window == request->requestor &&
		    event.xproperty.atom == request->property &&
		    event.xproperty.state == PropertyDelete)
		{
			return true;
		}
	}
	return false;
}

static void
transport_notify(Display *display, const XSelectionRequestEvent *request,
    Atom property)
{
	XEvent event = {.xselection = {.type = SelectionNotify,
	    .requestor = request->requestor, .selection = request->selection,
	    .target = request->target, .property = property,
	    .time = request->time}};

	assert(FSendEvent(display, request->requestor, False, 0, &event));
	XFlush(display);
}

static void
transport_direct(Display *display, const XSelectionRequestEvent *request,
    Atom type, int format, const unsigned char *data, int count)
{
	XChangeProperty(display, request->requestor, request->property,
	    type, format, PropModeReplace, data, count);
	transport_notify(display, request, request->property);
	XSync(display, False);
}

static void
transport_mismatch(Display *display, const XSelectionRequestEvent *request,
    transport_scenario scenario)
{
	XEvent event = {.xselection = {.type = SelectionNotify,
	    .requestor = request->requestor, .selection = request->selection,
	    .target = request->target, .property = request->property,
	    .time = request->time}};

	XChangeProperty(display, request->requestor, request->property,
	    request->target, 8, PropModeReplace, (unsigned char *)"wrong", 5);
	switch (scenario)
	{
	case TRANSPORT_WRONG_REQUESTOR:
		event.xselection.requestor = DefaultRootWindow(display);
		break;
	case TRANSPORT_WRONG_SELECTION:
		event.xselection.selection = XA_SECONDARY;
		break;
	case TRANSPORT_WRONG_TARGET:
		event.xselection.target = XA_ATOM;
		break;
	case TRANSPORT_WRONG_PROPERTY:
		event.xselection.property = XA_CUT_BUFFER0;
		break;
	case TRANSPORT_WRONG_TIME:
		event.xselection.time++;
		break;
	case TRANSPORT_WRONG_NONE:
		event.xselection.selection = XA_SECONDARY;
		event.xselection.property = None;
		break;
	default:
		assert(false);
	}
	/* Destination is always the real requestor: even a corrupted requestor
	 * field is delivered to the connection actually running the new API. */
	assert(FSendEvent(display, request->requestor, False, 0, &event));
	XSync(display, False);
	while (transport_event(display, &event))
	{
		if (event.type == DestroyNotify)
		{
			return;
		}
	}
}

/* Split at the full packed metadata cap. UTF8 arrives only in the final
 * nonempty chunk, so choosing STRING before complete negotiation is visible. */
static bool
transport_targets_incr(Display *display,
    const XSelectionRequestEvent *request, transport_scenario scenario,
    Atom utf8)
{
	Atom incr = XInternAtom(display, "INCR", False);
	unsigned long indication = scenario == TRANSPORT_TARGETS_INCR_HINT ?
	    1025 : 1024;
	unsigned long atoms[129];

	for (size_t i = 0; i < 129; i++)
	{
		atoms[i] = XA_STRING;
	}
	XChangeProperty(display, request->requestor, request->property, incr,
	    32, PropModeReplace, (unsigned char *)&indication, 1);
	XSync(display, False);
	transport_notify(display, request, request->property);
	if (scenario == TRANSPORT_TARGETS_INCR_HINT)
	{
		assert(!transport_deleted(display, request));
		return false;
	}
	assert(transport_deleted(display, request));
	for (unsigned int i = 0; i < 2; i++)
	{
		Atom type = scenario == TRANSPORT_TARGETS_INCR_TYPE ?
		    XA_INTEGER : XA_ATOM;
		int count = scenario == TRANSPORT_TARGETS_INCR_AGGREGATE &&
		    i == 1 ? 129 : 128;

		if (i == 1)
		{
			atoms[count - 1] = utf8;
		}
		XChangeProperty(display, request->requestor, request->property,
		    type, 32, PropModeReplace, (unsigned char *)atoms, count);
		XFlush(display);
		if (!transport_deleted(display, request))
		{
			assert(scenario == TRANSPORT_TARGETS_INCR_TYPE ||
			    scenario == TRANSPORT_TARGETS_INCR_AGGREGATE);
			return false;
		}
	}
	int format = scenario == TRANSPORT_TARGETS_INCR_TERMINAL ? 8 : 32;
	XChangeProperty(display, request->requestor, request->property,
	    XA_ATOM, format, PropModeReplace, (unsigned char *)atoms, 0);
	XSync(display, False);
	return scenario == TRANSPORT_TARGETS_INCR;
}

static void
transport_incremental(Display *display,
    const XSelectionRequestEvent *request, transport_scenario scenario,
    Atom cancel, Window service_window)
{
	Atom incr = XInternAtom(display, "INCR", False);
	unsigned long indication[2] = {64, 0};
	int count = 1, format = 32;

	if (scenario == TRANSPORT_INCR_FLOOD)
	{
		XEvent marker = {.xclient = {.type = ClientMessage,
		    .window = service_window, .message_type = cancel,
		    .format = 32}};

		marker.xclient.data.l[0] = request->requestor;
		marker.xclient.data.l[1] = 1;
		assert(FSendEvent(display, service_window, False, 0, &marker));
	}
	if (scenario == TRANSPORT_INCR_HEADER_ZERO)
	{
		count = 0;
	}
	else if (scenario == TRANSPORT_INCR_HEADER_TWO)
	{
		count = 2;
	}
	else if (scenario == TRANSPORT_INCR_HEADER_FORMAT)
	{
		format = 8;
	}
	else if (scenario == TRANSPORT_INCR_HEADER_LARGE)
	{
		indication[0] = FSELECTION_TEXT_MAX_BYTES + 1;
	}
	XChangeProperty(display, request->requestor, request->property, incr,
	    format, PropModeReplace, (unsigned char *)indication, count);
	/* Guarantee the header's real NewValue precedes SelectionNotify. */
	XSync(display, False);
	transport_notify(display, request, request->property);
	if (scenario >= TRANSPORT_INCR_HEADER_ZERO &&
	    scenario <= TRANSPORT_INCR_HEADER_LARGE)
	{
		assert(!transport_deleted(display, request));
		return;
	}
	bool acknowledged = transport_deleted(display, request);
	if (!acknowledged && scenario == TRANSPORT_INCR_FLOOD)
	{
		/* The total deadline may expire before any nonempty chunk. */
		return;
	}
	assert(acknowledged);
	if (scenario == TRANSPORT_INCR_DEATH)
	{
		return;
	}
	if (scenario == TRANSPORT_INCR_REPEATED)
	{
		/* A repeated notification without a property is not a chunk. */
		XEvent fake = {.xproperty = {.type = PropertyNotify,
		    .window = request->requestor, .atom = request->property,
		    .state = PropertyNewValue}};
		assert(FSendEvent(display, request->requestor, False,
		    PropertyChangeMask, &fake));
		XFlush(display);
		assert(!transport_deleted(display, request));
		return;
	}
	if (scenario == TRANSPORT_INCR_FOREIGN)
	{
		XEvent foreign = {.xproperty = {.type = PropertyNotify,
		    .window = request->requestor, .atom = XA_CUT_BUFFER1,
		    .state = PropertyNewValue}};
		assert(FSendEvent(display, request->requestor, False,
		    PropertyChangeMask, &foreign));
	}
	unsigned char full[FSELECTION_TEXT_MAX_BYTES];
	memset(full, 'x', sizeof(full));
	const unsigned char *parts[] = {
	    (const unsigned char *)"A\xe2",
	    (const unsigned char *)"\x82", (const unsigned char *)"\xacZ"};
	int sizes[] = {2, 1, 2};
	unsigned char raw[] = {'a', 0, 'b', 0xff, 0};
	unsigned long word = 1;
	unsigned int chunks = scenario == TRANSPORT_INCR_EMPTY ? 0 : 3;

	if (scenario == TRANSPORT_INCR_EXACT)
	{
		parts[0] = raw;
		sizes[0] = sizeof(raw);
		chunks = 1;
	}
	if (scenario == TRANSPORT_INCR_MAX ||
	    scenario == TRANSPORT_INCR_AGGREGATE)
	{
		parts[0] = full;
		sizes[0] = sizeof(full);
		parts[1] = (unsigned char *)"x";
		sizes[1] = 1;
		chunks = scenario == TRANSPORT_INCR_MAX ? 1 : 2;
	}
	if (scenario == TRANSPORT_INCR_FLOOD)
	{
		chunks = 4097;
	}
	for (unsigned int i = 0; i < chunks; i++)
	{
		Atom type = request->target;
		format = 8;
		const unsigned char *data = parts[i % 3];
		count = sizes[i % 3];
		if (scenario == TRANSPORT_INCR_FLOOD)
		{
			data = (unsigned char *)"x";
			count = 1;
		}
		if (scenario == TRANSPORT_INCR_CHUNK_TYPE)
		{
			type = XA_ATOM;
		}
		if (scenario == TRANSPORT_INCR_CHUNK_FORMAT)
		{
			format = 32;
			data = (unsigned char *)&word;
			count = 1;
		}
		XChangeProperty(display, request->requestor, request->property,
		    type, format, PropModeReplace, data, count);
		XFlush(display);
		if (!transport_deleted(display, request))
		{
			assert(scenario == TRANSPORT_INCR_CHUNK_TYPE ||
			    scenario == TRANSPORT_INCR_CHUNK_FORMAT ||
			    scenario == TRANSPORT_INCR_AGGREGATE ||
			    scenario == TRANSPORT_INCR_FLOOD);
			return;
		}
	}
	if (scenario == TRANSPORT_INCR_MISSING_TERMINAL ||
	    scenario == TRANSPORT_CANCEL)
	{
		if (scenario == TRANSPORT_CANCEL)
		{
			XEvent event = {.xclient = {.type = ClientMessage,
			    .window = service_window, .message_type = cancel,
			    .format = 32}};
			event.xclient.data.l[0] = request->requestor;
			assert(FSendEvent(display, service_window,
			    False, 0, &event));
			XFlush(display);
		}
		assert(!transport_deleted(display, request));
		return;
	}
	Atom terminal = scenario == TRANSPORT_INCR_TERMINAL_TYPE ?
	    XA_ATOM : request->target;
	format = scenario == TRANSPORT_INCR_TERMINAL_FORMAT ? 32 : 8;
	XChangeProperty(display, request->requestor, request->property,
	    terminal, format, PropModeReplace, (unsigned char *)&word, 0);
	XSync(display, False);
}

static pid_t
transport_owner(Display *parent, Atom selection, transport_scenario scenario,
    Window service_window, Atom auxiliary, Atom cancel, bool text)
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
		/* The late case deliberately touches a retired requestor. */
		XSetErrorHandler(ignore_error);
		Window window = XCreateSimpleWindow(display,
		    DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
		Atom targets = XInternAtom(display, "TARGETS", False);
		Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
		XSetSelectionOwner(display, selection, window, CurrentTime);
		XSync(display, False);
		assert(XGetSelectionOwner(display, selection) == window);
		assert(write(ready[1], "r", 1) == 1);
		close(ready[1]);
		XEvent event;
		Window requestors[3] = {None};
		unsigned int attempts = 0;
		while (transport_event(display, &event))
		{
			if (event.type != SelectionRequest)
			{
				continue;
			}
			XSelectionRequestEvent request =
			    event.xselectionrequest;
			assert(text || request.target != targets);
			assert(attempts < 3);
			for (unsigned int i = 0; i < attempts; i++)
			{
				assert(requestors[i] != request.requestor);
			}
			requestors[attempts++] = request.requestor;
			XSelectInput(display, request.requestor,
			    PropertyChangeMask | StructureNotifyMask);
			if (request.target == targets)
			{
				assert(attempts == 1);
				if (scenario >= TRANSPORT_TARGETS_INCR &&
				    scenario <= TRANSPORT_TARGETS_INCR_TERMINAL)
				{
					if (!transport_targets_incr(display,
					    &request, scenario, utf8))
					{
						break;
					}
					continue;
				}
				if (scenario == TRANSPORT_LEGACY ||
				    scenario == TRANSPORT_DEADLINE)
				{
					if (scenario == TRANSPORT_DEADLINE)
					{
						transport_delay(100);
					}
					transport_notify(display,
					    &request, None);
					continue;
				}
				unsigned long atoms[257];
				for (size_t i = 0; i < 257; i++)
				{
					atoms[i] = XA_STRING;
				}
				atoms[0] = utf8;
				if (scenario == TRANSPORT_STRING)
				{
					atoms[0] = XA_STRING;
				}
				Atom type = scenario == TRANSPORT_TARGETS_TYPE ?
				    XA_INTEGER : XA_ATOM;
				int format =
				    scenario == TRANSPORT_TARGETS_FORMAT ?
				    8 : 32;
				int count =
				    scenario == TRANSPORT_TARGETS_LARGE ?
				    257 : 2;
				if (scenario == TRANSPORT_TARGETS_EMPTY)
				{
					count = 0;
				}
				if (scenario == TRANSPORT_UTF8_ONLY_REFUSED)
				{
					count = 1;
				}
				transport_direct(display, &request, type,
				    format, (unsigned char *)atoms, count);
				if (scenario >= TRANSPORT_TARGETS_TYPE &&
				    scenario <= TRANSPORT_TARGETS_EMPTY)
				{
					break;
				}
				continue;
			}
			if (request.target == utf8 &&
			    (scenario == TRANSPORT_LEGACY ||
				scenario == TRANSPORT_UTF8_REFUSED ||
				scenario == TRANSPORT_UTF8_ONLY_REFUSED ||
				scenario == TRANSPORT_DEADLINE))
			{
				if (scenario == TRANSPORT_DEADLINE)
				{
					transport_delay(100);
				}
				transport_notify(display, &request, None);
				if (scenario == TRANSPORT_UTF8_ONLY_REFUSED)
				{
					break;
				}
				continue;
			}
			if (scenario == TRANSPORT_REFUSED)
			{
				transport_notify(display, &request, None);
				break;
			}
			if (scenario >= TRANSPORT_WRONG_REQUESTOR &&
			    scenario <= TRANSPORT_WRONG_NONE)
			{
				transport_mismatch(display, &request, scenario);
				break;
			}
			if ((scenario >= TRANSPORT_INCR &&
			    scenario <= TRANSPORT_CANCEL))
			{
				transport_incremental(display, &request,
				    scenario, cancel, service_window);
				break;
			}
			if (scenario == TRANSPORT_SERVICE)
			{
				XConvertSelection(display, auxiliary, XA_STRING,
				    auxiliary, window, CurrentTime);
				XFlush(display);
				bool answered = false;
				while (transport_event(display, &event))
				{
					if (event.type == SelectionNotify)
					{
						assert(auxiliary ==
						    event.xselection.property);
						answered = true;
						break;
					}
				}
				assert(answered);
			}
			if (scenario == TRANSPORT_SERVICE_DEADLINE)
			{
				assert(!transport_deleted(display, &request));
				break;
			}
			if (scenario == TRANSPORT_DEADLINE)
			{
				transport_delay(120);
			}
			if (scenario == TRANSPORT_LATE)
			{
				XSelectionRequestEvent first = request;
				do
				{
					assert(transport_event(display,
					    &event));
				} while (event.type != SelectionRequest);
				request = event.xselectionrequest;
				assert(first.requestor != request.requestor);
				transport_direct(display, &first,
				    first.target, 8,
				    (unsigned char *)"stale", 5);
			}
			unsigned char full[FSELECTION_TEXT_MAX_BYTES + 1];
			memset(full, 'x', sizeof(full));
			unsigned char raw[] = {'a', 0, 'b', 0xff, 0};
			const unsigned char *data = (unsigned char *)"hello";
			int count = 5;
			if (scenario == TRANSPORT_EXACT)
			{
				data = raw;
				count = sizeof(raw);
			}
			else if (scenario == TRANSPORT_STRING)
			{
				data = (unsigned char *)"\xff";
				count = 1;
			}
			else if (scenario == TRANSPORT_EMPTY)
			{
				count = 0;
			}
			else if (scenario == TRANSPORT_MAX ||
			    scenario == TRANSPORT_OVERSIZE)
			{
				data = full;
				count = FSELECTION_TEXT_MAX_BYTES +
				    (scenario == TRANSPORT_OVERSIZE);
			}
			else if (scenario == TRANSPORT_EMBEDDED_NUL)
			{
				data = (unsigned char *)"a\0b";
				count = 3;
			}
			else if (scenario == TRANSPORT_TRAILING_NUL)
			{
				data = (unsigned char *)"hello";
				count = 6;
			}
			else if (scenario == TRANSPORT_BAD_UTF8)
			{
				data = (unsigned char *)"\xe0\x80\x80";
				count = 3;
			}
			transport_direct(display, &request, request.target, 8,
			    data, count);
			break;
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

static Bool
transport_safe_event(Display *display, XEvent *event, XPointer context)
{
	const transport_service_context *service =
	    (const transport_service_context *)context;

	return (event->type == SelectionRequest &&
	    event->xselectionrequest.owner == service->window &&
	    event->xselectionrequest.selection == service->auxiliary) ||
	    (event->type == ClientMessage &&
		event->xclient.window == service->window &&
		event->xclient.message_type == service->cancel);
}

static bool
transport_service(void *context)
{
	transport_service_context *service = context;
	XEvent event;

	service->calls++;
	if (service->slow)
	{
		transport_delay(120);
	}
	unsigned int work = 0;
	while (work++ < 32 && FCheckIfEvent(service->display, &event,
	    transport_safe_event, (XPointer)service))
	{
		if (event.type == ClientMessage)
		{
			service->cancelled_requestor =
			    (Window)(uint32_t)event.xclient.data.l[0];
			return false;
		}
		transport_direct(service->display, &event.xselectionrequest,
		    XA_STRING, 8, (unsigned char *)"serviced", 8);
		service->served++;
	}
	return true;
}

static Bool
transport_time_event(Display *display, XEvent *event, XPointer context)
{
	const Window *window = (const Window *)context;

	return event->type == PropertyNotify &&
	    event->xproperty.window == *window;
}

static Time
transport_time(Display *display, Atom property)
{
	Window window = XCreateSimpleWindow(display, DefaultRootWindow(display),
	    0, 0, 1, 1, 0, 0, 0);
	XSelectInput(display, window, PropertyChangeMask);
	XChangeProperty(display, window, property, XA_STRING, 8,
	    PropModeReplace, (unsigned char *)"t", 1);
	XSync(display, False);
	XEvent event;
	assert(FCheckIfEvent(display, &event, transport_time_event,
	    (XPointer)&window));
	assert(event.xproperty.time != CurrentTime);
	XDestroyWindow(display, window);
	return event.xproperty.time;
}

static void
transport_case(Display *display, Atom selection, Atom property,
    transport_scenario scenario, bool text, FSelectionStatus expected,
    const char *name)
{
	transport_service_context service = {.display = display,
	    .window = XCreateSimpleWindow(display, DefaultRootWindow(display),
		0, 0, 1, 1, 0, 0, 0),
	    .auxiliary = XInternAtom(display,
		"FVWM_SELECTION_AUXILIARY", False),
	    .cancel = XInternAtom(display, "FVWM_SELECTION_CANCEL", False),
	    .slow = scenario == TRANSPORT_SERVICE_DEADLINE};
	XSetSelectionOwner(display, service.auxiliary, service.window,
	    CurrentTime);
	XSync(display, False);
	pid_t owner = transport_owner(display, selection, scenario,
	    service.window, service.auxiliary, service.cancel, text);
	Time time = transport_time(display, property);
	Atom custom = XInternAtom(display, "FVWM_SELECTION_CUSTOM", False);
	FSelectionResult result = {.data = (unsigned char *)1, .length = 99,
	    .type = XA_ATOM};
	int timeout = FSELECTION_TIMEOUT_MS;
	if ((scenario >= TRANSPORT_WRONG_REQUESTOR &&
	    scenario <= TRANSPORT_WRONG_NONE) ||
	    scenario == TRANSPORT_INCR_MISSING_TERMINAL ||
	    scenario == TRANSPORT_INCR_DEATH ||
	    scenario == TRANSPORT_SERVICE_DEADLINE ||
	    scenario == TRANSPORT_LATE)
	{
		timeout = 80;
	}
	if (scenario == TRANSPORT_DEADLINE)
	{
		timeout = 280;
	}
	FSelectionService callback = scenario == TRANSPORT_CANCEL ||
	    scenario == TRANSPORT_SERVICE || service.slow ?
	    transport_service : NULL;
	int64_t start = now_ms();
	FSelectionStatus status = text ?
	    FSelectionRequestTextNegotiated(display, selection, property, time,
		timeout, callback, &service, &result) :
	    FSelectionRequestExact(display, selection, custom, property, time,
		timeout, callback, &service, &result);
	int64_t elapsed = now_ms() - start;
	if (scenario == TRANSPORT_INCR_FLOOD)
	{
		assert(status == FSELECTION_TOO_LARGE ||
		    status == FSELECTION_TIMED_OUT);
		printf("typed INCR flood bound: %s\n",
		    status == FSELECTION_TOO_LARGE ? "nonempty chunk count" :
		    "absolute deadline; chunk limit not witnessed");
	}
	else
	{
		if (status != expected)
		{
			fprintf(stderr,
			    "typed selection %s status %d expected %d\n",
			    name, status, expected);
		}
		assert(status == expected);
	}
	if (status != FSELECTION_OK)
	{
		assert(result.data == NULL && result.length == 0 &&
		    result.type == None);
	}
	else
	{
		assert(result.data != NULL && result.data[result.length] == 0);
		if (text)
		{
			Atom expected_type = scenario == TRANSPORT_STRING ||
			    scenario == TRANSPORT_LEGACY ||
			    scenario == TRANSPORT_UTF8_REFUSED ? XA_STRING :
			    XInternAtom(display, "UTF8_STRING", False);
			assert(result.type == expected_type);
		}
		if (!text)
		{
			unsigned char raw[] = {'a', 0, 'b', 0xff, 0};
			assert(result.type == custom &&
			    result.length == sizeof(raw));
			assert(memcmp(result.data, raw, sizeof(raw)) == 0);
		}
		else if (scenario == TRANSPORT_STRING)
		{
			assert(result.type == XA_STRING && result.length == 1);
			assert(result.data[0] == 0xff);
		}
		else if (scenario == TRANSPORT_EMPTY ||
		    scenario == TRANSPORT_INCR_EMPTY)
		{
			assert(result.length == 0);
		}
		else if (scenario == TRANSPORT_MAX ||
		    scenario == TRANSPORT_INCR_MAX)
		{
			assert(result.length == FSELECTION_TEXT_MAX_BYTES);
			for (size_t i = 0; i < result.length; i++)
			{
				assert(result.data[i] == 'x');
			}
		}
		else if (scenario == TRANSPORT_INCR ||
		    scenario == TRANSPORT_INCR_FOREIGN)
		{
			assert(result.length == 5);
			assert(memcmp(result.data, "A\xe2\x82\xacZ", 5) == 0);
		}
		else
		{
			assert(result.length == 5 &&
			    memcmp(result.data, "hello", 5) == 0);
		}
	}
	FSelectionResultFree(&result);
	assert(result.data == NULL && result.length == 0 &&
	    result.type == None);
	if (scenario == TRANSPORT_SERVICE)
	{
		assert(service.calls > 0 && service.served == 1);
	}
	if (scenario == TRANSPORT_CANCEL)
	{
		XEvent retired;

		assert(service.cancelled_requestor != None);
		assert(!FCheckIfEvent(display, &retired, transport_time_event,
		    (XPointer)&service.cancelled_requestor));
	}
	if (expected == FSELECTION_TIMED_OUT)
	{
		assert(elapsed >= timeout - 10 && elapsed < 700);
	}
	if (scenario == TRANSPORT_INCR_FLOOD &&
	    status == FSELECTION_TIMED_OUT)
	{
		assert(elapsed >= timeout - 10);
	}
	if (scenario == TRANSPORT_LATE)
	{
		status = FSelectionRequestExact(display, selection, custom,
		    property,
		    CurrentTime, 500, NULL, NULL, &result);
		assert(status == FSELECTION_OK && result.length == 5);
		assert(memcmp(result.data, "hello", 5) == 0);
		FSelectionResultFree(&result);
	}
	int child_status;
	assert(waitpid(owner, &child_status, 0) == owner);
	assert(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
	if (scenario == TRANSPORT_INCR_FLOOD)
	{
		XEvent marker, retired;
		Window root, parent, *children = NULL;
		unsigned int count = 0;

		assert(FCheckIfEvent(display, &marker, transport_safe_event,
		    (XPointer)&service));
		assert(marker.type == ClientMessage &&
		    marker.xclient.data.l[1] == 1);
		Window requestor = (Window)(uint32_t)marker.xclient.data.l[0];
		assert(!FCheckIfEvent(display, &retired, transport_time_event,
		    (XPointer)&requestor));
		assert(XQueryTree(display, DefaultRootWindow(display), &root,
		    &parent, &children, &count));
		for (unsigned int i = 0; i < count; i++)
		{
			assert(children[i] != requestor);
		}
		XFree(children);
	}
	XDestroyWindow(display, service.window);
	printf("typed selection %s passed (%lld ms)\n", name,
	    (long long)elapsed);
}

static void
transport_tests(Display *display, Atom selection, Atom property)
{
	static const struct {
		const char *name;
		transport_scenario scenario;
		bool text;
		FSelectionStatus status;
	} cases[] = {
	    {"exact bytes", TRANSPORT_EXACT, false, FSELECTION_OK},
	    {"STRING encoding", TRANSPORT_STRING, true, FSELECTION_OK},
	    {"negotiated UTF8", TRANSPORT_UTF8, true, FSELECTION_OK},
	    {"empty direct", TRANSPORT_EMPTY, true, FSELECTION_OK},
	    {"direct cap", TRANSPORT_MAX, true, FSELECTION_OK},
	    {"direct cap+1", TRANSPORT_OVERSIZE, true, FSELECTION_TOO_LARGE},
	    {"embedded NUL", TRANSPORT_EMBEDDED_NUL, true, FSELECTION_INVALID},
	    {"trailing NUL", TRANSPORT_TRAILING_NUL, true, FSELECTION_OK},
	    {"invalid UTF8 no fallback",
		TRANSPORT_BAD_UTF8, true, FSELECTION_INVALID},
	    {"TARGETS type", TRANSPORT_TARGETS_TYPE, true, FSELECTION_INVALID},
	    {"TARGETS format",
		TRANSPORT_TARGETS_FORMAT, true, FSELECTION_INVALID},
	    {"TARGETS bound",
		TRANSPORT_TARGETS_LARGE, true, FSELECTION_TOO_LARGE},
	    {"TARGETS empty",
		TRANSPORT_TARGETS_EMPTY, true, FSELECTION_REFUSED},
	    {"TARGETS INCR split/cap", TRANSPORT_TARGETS_INCR,
		true, FSELECTION_OK},
	    {"TARGETS INCR hint", TRANSPORT_TARGETS_INCR_HINT,
		true, FSELECTION_TOO_LARGE},
	    {"TARGETS INCR aggregate", TRANSPORT_TARGETS_INCR_AGGREGATE,
		true, FSELECTION_TOO_LARGE},
	    {"TARGETS INCR type", TRANSPORT_TARGETS_INCR_TYPE,
		true, FSELECTION_INVALID},
	    {"TARGETS INCR terminal", TRANSPORT_TARGETS_INCR_TERMINAL,
		true, FSELECTION_INVALID},
	    {"legacy fallback", TRANSPORT_LEGACY, true, FSELECTION_OK},
	    {"advertised fallback",
		TRANSPORT_UTF8_REFUSED, true, FSELECTION_OK},
	    {"unadvertised STRING",
		TRANSPORT_UTF8_ONLY_REFUSED, true, FSELECTION_REFUSED},
	    {"exact refused", TRANSPORT_REFUSED, false, FSELECTION_REFUSED},
	    {"wrong requestor",
		TRANSPORT_WRONG_REQUESTOR, false, FSELECTION_TIMED_OUT},
	    {"wrong selection",
		TRANSPORT_WRONG_SELECTION, false, FSELECTION_TIMED_OUT},
	    {"wrong target",
		TRANSPORT_WRONG_TARGET, false, FSELECTION_TIMED_OUT},
	    {"wrong property",
		TRANSPORT_WRONG_PROPERTY, false, FSELECTION_TIMED_OUT},
	    {"wrong time", TRANSPORT_WRONG_TIME, false, FSELECTION_TIMED_OUT},
	    {"uncorrelated refusal",
		TRANSPORT_WRONG_NONE, false, FSELECTION_TIMED_OUT},
	    {"INCR split UTF8/header queued",
		TRANSPORT_INCR, true, FSELECTION_OK},
	    {"INCR empty", TRANSPORT_INCR_EMPTY, true, FSELECTION_OK},
	    {"INCR exact bytes", TRANSPORT_INCR_EXACT, false, FSELECTION_OK},
	    {"INCR exact cap", TRANSPORT_INCR_MAX, true, FSELECTION_OK},
	    {"INCR empty header",
		TRANSPORT_INCR_HEADER_ZERO, true, FSELECTION_INVALID},
	    {"INCR two-word header",
		TRANSPORT_INCR_HEADER_TWO, true, FSELECTION_TOO_LARGE},
	    {"INCR header format",
		TRANSPORT_INCR_HEADER_FORMAT, true, FSELECTION_INVALID},
	    {"INCR header cap",
		TRANSPORT_INCR_HEADER_LARGE, true, FSELECTION_TOO_LARGE},
	    {"INCR chunk type",
		TRANSPORT_INCR_CHUNK_TYPE, true, FSELECTION_INVALID},
	    {"INCR chunk format",
		TRANSPORT_INCR_CHUNK_FORMAT, true, FSELECTION_INVALID},
	    {"INCR aggregate cap",
		TRANSPORT_INCR_AGGREGATE, true, FSELECTION_TOO_LARGE},
	    {"INCR terminal type",
		TRANSPORT_INCR_TERMINAL_TYPE, true, FSELECTION_INVALID},
	    {"INCR terminal format",
		TRANSPORT_INCR_TERMINAL_FORMAT, true, FSELECTION_INVALID},
	    {"INCR missing terminal",
		TRANSPORT_INCR_MISSING_TERMINAL, true, FSELECTION_TIMED_OUT},
	    {"INCR chunk/deadline bound",
		TRANSPORT_INCR_FLOOD, false, FSELECTION_TOO_LARGE},
	    {"INCR foreign event", TRANSPORT_INCR_FOREIGN, true, FSELECTION_OK},
	    {"INCR repeated event",
		TRANSPORT_INCR_REPEATED, true, FSELECTION_INVALID},
	    {"INCR peer death",
		TRANSPORT_INCR_DEATH, true, FSELECTION_TIMED_OUT},
	    {"cancel partial INCR",
		TRANSPORT_CANCEL, true, FSELECTION_CANCELLED},
	    {"service independent owner",
		TRANSPORT_SERVICE, true, FSELECTION_OK},
	    {"deadline after callback",
		TRANSPORT_SERVICE_DEADLINE, true, FSELECTION_TIMED_OUT},
	    {"one negotiation/fallback deadline",
		TRANSPORT_DEADLINE, true, FSELECTION_TIMED_OUT},
	    {"late reply isolation",
		TRANSPORT_LATE, false, FSELECTION_TIMED_OUT}
	};
	FSelectionResult invalid = {.data = (unsigned char *)1,
	    .length = 7, .type = XA_ATOM};
	assert(FSelectionRequestExact(NULL, selection, XA_STRING, property,
	    CurrentTime, 100, NULL, NULL, &invalid) == FSELECTION_INVALID);
	assert(invalid.data == NULL && invalid.length == 0 &&
	    invalid.type == None);
	FSelectionResultFree(NULL);
	XEvent expose;
	for (int i = 3; i >= 1; i--)
	{
		expose = (XEvent){.xexpose = {.type = Expose,
		    .window = DefaultRootWindow(display), .count = i}};
		FPutBackEvent(display, &expose);
	}
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		transport_case(display, selection, property, cases[i].scenario,
		    cases[i].text, cases[i].status, cases[i].name);
	}
	for (int i = 1; i <= 3; i++)
	{
		assert(FCheckTypedEvent(display, Expose, &expose));
		assert(expose.xexpose.count == i);
	}
	puts("typed transport preserves unrelated FIFO input and servicing");
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
	transport_tests(display, selection, property);
	XCloseDisplay(display);
	puts("selection isolation, event preservation and deadline passed");
	return 0;
}
