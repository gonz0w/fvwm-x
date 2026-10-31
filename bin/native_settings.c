/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "native_settings_policy.h"
#include "libs/FProperty.h"
#include "libs/XError.h"
#include <X11/Xatom.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct
{
	Display *display;
	Window root, owner;
	Atom selection, settings, manager, timestamp, resources;
	ns_model model;
	bool claimed, published, acquiring, stopping, output_failed;
	bool resource_conflict, resource_missing;
	int64_t deadline, next_attempt;
	uint32_t published_serial;
	const char *xstate, *resource_state, *reason, *resource_reason;
	char input[NS_LINE_MAX];
	size_t input_used;
	char output[256];
	size_t output_used, output_sent;
	char owned[NS_BLOCK_MAX];
	size_t owned_length;
} ns_runtime;

/* The helper has one X thread. Foreign-window probes fence previous requests
 * and allow only BadWindow for the exact probed XID and request interval. */
static struct
{
	Display *display;
	Window window;
	unsigned long first;
	bool active, vanished, failed;
} ns_xtrap;
static bool ns_xfailed;

static int
native_x_error(Display *display, XErrorEvent *event)
{
	(void)display;
	(void)event;
	ns_xfailed = true;
	if (ns_xtrap.active)
		ns_xtrap.failed = true;
	return 0;
}

static int
native_x_io_error(Display *display)
{
	(void)display;
	/* The server connection cannot support cleanup after an I/O failure. */
	fputs("fvwm-native-settings: X connection failed\n", stderr);
	_exit(1);
}

static int
foreign_x_error(Display *display, XErrorEvent *event)
{
	if (ns_xtrap.active && display == ns_xtrap.display &&
	    event->error_code == BadWindow &&
	    event->resourceid == ns_xtrap.window &&
	    event->serial >= ns_xtrap.first)
	{
		ns_xtrap.vanished = true;
		return 0;
	}
	return ferror_call_next_error_handler(display, event);
}

static bool
owner_gone(ns_runtime *runtime, Window owner)
{
	XWindowAttributes attributes;

	XSync(runtime->display, False);
	if (ns_xfailed)
		return false;
	ns_xtrap.display = runtime->display;
	ns_xtrap.window = owner;
	ns_xtrap.first = NextRequest(runtime->display);
	ns_xtrap.active = true;
	ns_xtrap.vanished = false;
	ns_xtrap.failed = false;
	ferror_set_temp_error_handler(foreign_x_error);
	Status status = XGetWindowAttributes(runtime->display, owner,
	    &attributes);
	XSync(runtime->display, False);
	ferror_reset_temp_error_handler();
	ns_xtrap.active = false;
	return !status && ns_xtrap.vanished && !ns_xtrap.failed && !ns_xfailed;
}

static int64_t
now_milliseconds(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return -1;
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static bool
nonblocking(int descriptor)
{
	int flags = fcntl(descriptor, F_GETFL);

	return flags >= 0 &&
	    fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == 0;
}

static void
queue_result(ns_runtime *runtime, uint64_t sequence,
	const char *state, const char *reason)
{
	if (!runtime->model.initialized || runtime->output_used)
		return;
	int length = snprintf(runtime->output, sizeof(runtime->output),
	    "FNS1 RESULT %s %s %" PRIu64 " %s %s %" PRIu32 " %s\n",
	    runtime->model.current.desktop, runtime->model.current.run,
	    sequence, state, runtime->resource_state,
	    runtime->published ? runtime->published_serial : 0, reason);
	if (length <= 0 || (size_t)length >= sizeof(runtime->output))
	{
		runtime->output_failed = true;
		runtime->stopping = true;
		return;
	}
	runtime->output_used = (size_t)length;
	runtime->output_sent = 0;
}

static void
stop_runtime(ns_runtime *runtime, const char *state,
	const char *reason)
{
	if (runtime->stopping)
		return;
	runtime->stopping = true;
	runtime->xstate = state;
	runtime->reason = reason;
}

static FPropertyResult
read_resources(ns_runtime *runtime,
	unsigned char **data, size_t *length)
{
	FPropertyLimits limits = {0, NS_RESOURCE_MAX, 8};

	return FPropertyRead(runtime->display, runtime->root,
	    runtime->resources, XA_STRING, limits, data, length);
}

static void
resource_refusal(ns_runtime *runtime, const char *reason)
{
	runtime->resource_conflict = true;
	runtime->resource_state = "conflict";
	runtime->reason = runtime->resource_reason = reason;
}

static void
apply_resources(ns_runtime *runtime, bool cleanup)
{
	unsigned char *before = NULL, *fresh = NULL;
	size_t length = 0, fresh_length = 0, start = 0, remove_length = 0;
	char output[NS_RESOURCE_MAX], block[NS_BLOCK_MAX];
	size_t block_length = 0;
	bool found = false, orphan = false;
	ns_block previous;

	/* A latched conflict forbids updates, but cleanup can still remove the
	 * exact block if another writer has merely appended a tail. */
	if (runtime->resource_conflict && !cleanup)
	{
		runtime->reason = runtime->resource_reason;
		return;
	}
	if (cleanup && !runtime->owned_length)
		return;
	FPropertyResult result = read_resources(runtime, &before, &length);
	if (result != FPROPERTY_OK && result != FPROPERTY_MISSING)
	{
		runtime->resource_state = result == FPROPERTY_TOO_LARGE ?
		    "conflict" : "unavailable";
		runtime->reason = result == FPROPERTY_TOO_LARGE ?
		    "resource-limit" : "x-error";
		runtime->resource_conflict = true;
		runtime->resource_reason = runtime->reason;
		goto out;
	}
	/* Missing properties have a NULL output. Use a valid empty span. */
	const char *bytes = before ? (const char *)before : "";
	if (runtime->owned_length)
	{
		if (!ns_resource_owned(bytes, length, runtime->owned,
		    runtime->owned_length, cleanup, &start))
		{
			resource_refusal(runtime, "resource-conflict");
			goto out;
		}
		remove_length = runtime->owned_length;
	}
	else
	{
		ns_resource_result scan = ns_resource_scan(bytes, length,
		    &previous, &found);
		if (scan != NS_RESOURCE_OK)
		{
			resource_refusal(runtime, scan == NS_RESOURCE_LIMIT ?
			    "resource-limit" : "resource-conflict");
			goto out;
		}
		if (found)
		{
			/* Confirm absence in the compare/write transaction.
			 * A reused/live XID is deliberately never adopted. */
			if (!owner_gone(runtime, previous.owner))
			{
				resource_refusal(runtime, "resource-conflict");
				goto out;
			}
			start = previous.start;
			remove_length = previous.length;
			orphan = true;
		}
		else
			start = length;
		runtime->resource_missing = result == FPROPERTY_MISSING;
	}
	size_t base_length = length - remove_length;
	memcpy(output, bytes, start);
	memcpy(output + start, bytes + start + remove_length,
	    length - start - remove_length);
	if (!cleanup)
	{
		bool separator = base_length && output[base_length - 1] != '\n';
		block_length = ns_resource_block(&runtime->model.current,
		    (uint32_t)runtime->owner, separator, block, sizeof(block));
		if (block_length > NS_RESOURCE_MAX - base_length)
		{
			resource_refusal(runtime, "resource-limit");
			goto out;
		}
		memcpy(output + base_length, block, block_length);
	}
	size_t output_length = base_length + block_length;
	/* All parsing, allocation and construction precedes the grab. Only
	 * bounded reads, owner-death confirmation and compare/write remain. */
	XGrabServer(runtime->display);
	FPropertyResult fresh_result = read_resources(runtime, &fresh,
	    &fresh_length);
	bool unchanged = fresh_result == result && fresh_length == length &&
	    (!length || (fresh && !memcmp(before, fresh, length)));
	if (unchanged && orphan)
		unchanged = owner_gone(runtime, previous.owner);
	if (!unchanged || ns_xfailed)
	{
		XUngrabServer(runtime->display);
		XSync(runtime->display, False);
		resource_refusal(runtime, "resource-conflict");
		goto out;
	}
	if (output_length != length ||
	    (length && memcmp(output, before, length)))
	{
		if (!output_length && runtime->resource_missing)
			XDeleteProperty(runtime->display, runtime->root,
			    runtime->resources);
		else
			XChangeProperty(runtime->display, runtime->root,
			    runtime->resources, XA_STRING, 8, PropModeReplace,
			    (unsigned char *)output, (int)output_length);
	}
	XUngrabServer(runtime->display);
	XSync(runtime->display, False);
	if (ns_xfailed)
	{
		runtime->resource_state = "unavailable";
		runtime->reason = runtime->resource_reason = "x-error";
		runtime->resource_conflict = true;
		goto out;
	}
	memcpy(runtime->owned, block, block_length);
	runtime->owned_length = block_length;
	runtime->resource_state = block_length ? "applied" : "inherited";
out:
	free(before);
	free(fresh);
}

static Time
server_timestamp(ns_runtime *runtime)
{
	unsigned char byte = 1;
	XEvent event;
	Time timestamp = CurrentTime;

	XChangeProperty(runtime->display, runtime->owner, runtime->timestamp,
	    XA_INTEGER, 8, PropModeReplace, &byte, 1);
	XSync(runtime->display, False);
	for (unsigned int i = 0; i < 16 &&
	    XCheckWindowEvent(runtime->display, runtime->owner,
	    PropertyChangeMask, &event); i++)
		if (event.type == PropertyNotify &&
		    event.xproperty.atom == runtime->timestamp)
			timestamp = event.xproperty.time;
	return timestamp;
}

static bool
publish_settings(ns_runtime *runtime)
{
	unsigned char data[NS_LINE_MAX];
	size_t length = ns_encode(&runtime->model, data, sizeof(data));

	if (!length || !runtime->claimed ||
	    XGetSelectionOwner(runtime->display, runtime->selection) !=
	    runtime->owner)
	{
		stop_runtime(runtime, "lost", "owner-conflict");
		return false;
	}
	XChangeProperty(runtime->display, runtime->owner, runtime->settings,
	    runtime->settings, 8, PropModeReplace, data, (int)length);
	XSync(runtime->display, False);
	if (ns_xfailed)
	{
		stop_runtime(runtime, "failed", "x-error");
		return false;
	}
	runtime->published = true;
	runtime->published_serial = runtime->model.serial;
	return true;
}

static void
acquire_selection(ns_runtime *runtime, int64_t now)
{
	if (now >= runtime->deadline)
	{
		stop_runtime(runtime, "conflict", "owner-conflict");
		return;
	}
	Time timestamp = server_timestamp(runtime);
	if (!timestamp || ns_xfailed)
	{
		stop_runtime(runtime, "failed", "x-error");
		return;
	}
	XGrabServer(runtime->display);
	if (XGetSelectionOwner(runtime->display, runtime->selection) == None)
	{
		XSetSelectionOwner(runtime->display, runtime->selection,
		    runtime->owner, timestamp);
		runtime->claimed = XGetSelectionOwner(runtime->display,
		    runtime->selection) == runtime->owner;
	}
	XUngrabServer(runtime->display);
	XSync(runtime->display, False);
	if (ns_xfailed)
	{
		stop_runtime(runtime, "failed", "x-error");
		return;
	}
	runtime->next_attempt = now + 50;
	if (!runtime->claimed)
		return;
	if (!publish_settings(runtime))
		return;
	XEvent event = {0};
	event.xclient.type = ClientMessage;
	event.xclient.window = runtime->root;
	event.xclient.message_type = runtime->manager;
	event.xclient.format = 32;
	event.xclient.data.l[0] = timestamp;
	event.xclient.data.l[1] = runtime->selection;
	event.xclient.data.l[2] = runtime->owner;
	XSendEvent(runtime->display, runtime->root, False, StructureNotifyMask,
	    &event);
	XSync(runtime->display, False);
	if (ns_xfailed)
	{
		stop_runtime(runtime, "failed", "x-error");
		return;
	}
	runtime->acquiring = false;
	runtime->xstate = "ready";
	runtime->reason = "none";
	apply_resources(runtime, false);
	queue_result(runtime, runtime->model.current.sequence, "ready",
	    runtime->reason);
}

static bool
start_display(ns_runtime *runtime)
{
	/* The initial tuple has already passed all pure validation. */
	runtime->display = XOpenDisplay(NULL);
	if (!runtime->display)
		return false;
	XSetErrorHandler(native_x_error);
	XSetIOErrorHandler(native_x_io_error);
	runtime->root = DefaultRootWindow(runtime->display);
	char name[64];
	snprintf(name, sizeof(name), "_XSETTINGS_S%d",
	    DefaultScreen(runtime->display));
	runtime->selection = XInternAtom(runtime->display, name, False);
	runtime->settings = XInternAtom(runtime->display,
	    "_XSETTINGS_SETTINGS", False);
	runtime->manager = XInternAtom(runtime->display, "MANAGER", False);
	runtime->timestamp = XInternAtom(runtime->display,
	    "_FVWM_NATIVE_SETTINGS_TIMESTAMP", False);
	runtime->resources = XA_RESOURCE_MANAGER;
	XSetWindowAttributes attributes = {0};
	attributes.override_redirect = True;
	attributes.event_mask = PropertyChangeMask | StructureNotifyMask;
	runtime->owner = XCreateWindow(runtime->display, runtime->root,
	    -1, -1, 1, 1, 0, 0, InputOnly, CopyFromParent,
	    CWOverrideRedirect | CWEventMask, &attributes);
	XSync(runtime->display, False);
	if (ns_xfailed)
		return false;
	int64_t now = now_milliseconds();
	if (now < 0)
		return false;
	runtime->acquiring = true;
	runtime->deadline = now + 2000;
	runtime->next_attempt = now;
	return true;
}

static void
apply_line(ns_runtime *runtime)
{
	ns_request request;

	if (!ns_parse(runtime->input, runtime->input_used, &request))
	{
		/* Malformed tuples have no trustworthy correlation. Retire
		 * using the last accepted identity without publishing them. */
		stop_runtime(runtime, "failed", "invalid-input");
		return;
	}
	if (!runtime->model.initialized && request.sequence != 1)
	{
		stop_runtime(runtime, "failed", "invalid-input");
		return;
	}
	ns_admission admission = ns_admit(&runtime->model, &request);
	if (admission == NS_STALE)
	{
		queue_result(runtime, request.sequence, runtime->xstate,
		    "stale-input");
		return;
	}
	if (admission == NS_DUPLICATE)
	{
		if (!runtime->acquiring)
			queue_result(runtime, request.sequence, runtime->xstate,
			    runtime->reason);
		return;
	}
	if (runtime->acquiring)
	{
		stop_runtime(runtime, "failed", "invalid-input");
		return;
	}
	bool initial = !runtime->model.initialized;
	bool changed = ns_commit(&runtime->model, &request);
	if (initial)
	{
		if (!start_display(runtime))
			stop_runtime(runtime, "failed", "x-error");
		return;
	}
	if (changed && !publish_settings(runtime))
		return;
	runtime->reason = "none";
	/* Even an unchanged tuple must observe external overlay conflicts,
	 * without generating a property write for unchanged bytes. */
	apply_resources(runtime, false);
	queue_result(runtime, request.sequence, "ready", runtime->reason);
}

static void
read_input_byte(ns_runtime *runtime)
{
	char byte;
	ssize_t received = read(STDIN_FILENO, &byte, 1);

	if (received == 0)
	{
		stop_runtime(runtime, "off", "parent-eof");
		return;
	}
	if (received < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			stop_runtime(runtime, "failed", "invalid-input");
		return;
	}
	if (runtime->input_used == sizeof(runtime->input))
	{
		stop_runtime(runtime, "failed", "invalid-input");
		return;
	}
	runtime->input[runtime->input_used++] = byte;
	if (byte == '\n')
	{
		apply_line(runtime);
		runtime->input_used = 0;
	}
}

static void
write_output(ns_runtime *runtime)
{
	if (!runtime->output_used || runtime->output_failed)
		return;
	ssize_t written = write(STDOUT_FILENO,
	    runtime->output + runtime->output_sent,
	    runtime->output_used - runtime->output_sent);
	if (written < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
		{
			runtime->output_failed = true;
			stop_runtime(runtime, "off", "parent-eof");
		}
		return;
	}
	runtime->output_sent += (size_t)written;
	if (runtime->output_sent == runtime->output_used)
		runtime->output_used = runtime->output_sent = 0;
}

static void
process_x_event(ns_runtime *runtime)
{
	XEvent event;

	XNextEvent(runtime->display, &event);
	if (event.type == SelectionClear &&
	    event.xselectionclear.selection == runtime->selection &&
	    event.xselectionclear.window == runtime->owner)
	{
		runtime->claimed = false;
		stop_runtime(runtime, "lost", "owner-conflict");
	}
}

static void
cleanup_runtime(ns_runtime *runtime)
{
	if (!runtime->display)
		return;
	apply_resources(runtime, true);
	XGrabServer(runtime->display);
	if (runtime->claimed &&
	    XGetSelectionOwner(runtime->display, runtime->selection) ==
	    runtime->owner)
		XSetSelectionOwner(runtime->display, runtime->selection, None,
		    CurrentTime);
	XUngrabServer(runtime->display);
	if (runtime->owner)
		XDestroyWindow(runtime->display, runtime->owner);
	XSync(runtime->display, False);
	XCloseDisplay(runtime->display);
	runtime->display = NULL;
}

int
main(int argc, char **argv)
{
	ns_runtime runtime = {0};
	(void)argv;

	if (argc != 1)
	{
		fputs("fvwm-native-settings: invalid arguments\n", stderr);
		return 2;
	}
	if (!nonblocking(STDIN_FILENO) || !nonblocking(STDOUT_FILENO))
	{
		fputs("fvwm-native-settings: pipe setup failed\n", stderr);
		return 1;
	}
	signal(SIGPIPE, SIG_IGN);
	runtime.xstate = "off";
	runtime.resource_state = "unavailable";
	runtime.reason = runtime.resource_reason = "none";
	while (!runtime.stopping)
	{
		int64_t started = now_milliseconds();
		if (started < 0)
		{
			stop_runtime(&runtime, "failed", "x-error");
			break;
		}
		struct pollfd descriptors[3] = {
			{STDIN_FILENO, runtime.output_used ? 0 : POLLIN, 0},
			{STDOUT_FILENO, runtime.output_used ? POLLOUT : 0, 0},
			{runtime.display ?
			    ConnectionNumber(runtime.display) : -1, POLLIN, 0}
		};
		bool queued_x = runtime.display && XPending(runtime.display);
		int timeout = queued_x ? 0 : (runtime.acquiring ? 20 : -1);
		int status = poll(descriptors, 3, timeout);
		if (status < 0 && errno != EINTR)
		{
			stop_runtime(&runtime, "failed", "invalid-input");
			break;
		}
		if (descriptors[0].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			stop_runtime(&runtime, "off", "parent-eof");
			break;
		}
		if (descriptors[1].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			runtime.output_failed = true;
			stop_runtime(&runtime, "off", "parent-eof");
			break;
		}
		if (descriptors[2].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			stop_runtime(&runtime, "failed", "x-error");
			break;
		}
		if (descriptors[1].revents & POLLOUT)
			write_output(&runtime);
		/* Reserve 16 more events for a timestamp acquisition below.
		 * The time target cannot preempt a blocking Xlib call. */
		int64_t turn = now_milliseconds();
		for (unsigned int i = 0; i < 16 && !runtime.stopping; i++)
		{
			if (runtime.display && XPending(runtime.display))
				process_x_event(&runtime);
			else if (!runtime.output_used &&
			    (descriptors[0].revents & POLLIN))
				read_input_byte(&runtime);
			else
				break;
			int64_t now = now_milliseconds();
			if (now < 0 || now - turn >= 2)
				break;
		}
		int64_t now = now_milliseconds();
		if (now < 0 || ns_xfailed)
			stop_runtime(&runtime, "failed", "x-error");
		else if (runtime.acquiring && now >= runtime.next_attempt &&
		    now - turn < 2 && !runtime.stopping)
			acquire_selection(&runtime, now);
	}
	const char *terminal_reason = runtime.reason;
	cleanup_runtime(&runtime);
	/* Terminal completion is retained separately from the one bounded
	 * result slot. EOF may leave no reader; never block exit on stdout. */
	int64_t finish = now_milliseconds();
	bool terminal_queued = false;
	for (unsigned int i = 0; i < 32 && !runtime.output_failed; i++)
	{
		write_output(&runtime);
		if (!runtime.output_used && !terminal_queued)
		{
			queue_result(&runtime, runtime.model.current.sequence,
			    runtime.xstate, terminal_reason);
			terminal_queued = true;
			write_output(&runtime);
		}
		if (!runtime.output_used || now_milliseconds() - finish >= 100)
			break;
		struct pollfd descriptor = {STDOUT_FILENO, POLLOUT, 0};
		if (poll(&descriptor, 1, 5) < 0 && errno != EINTR)
			break;
	}
	if (!runtime.model.initialized && strcmp(runtime.reason, "parent-eof"))
		fputs("fvwm-native-settings: invalid input\n", stderr);
	return !strcmp(runtime.xstate, "off") ? 0 : 1;
}
