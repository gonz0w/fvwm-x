/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/content_selection.h"
#include <X11/Xatom.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Independent owner and public pattern, never private clipboard capture.
 * Synthetic source generations/clocks exercise the callback seam; they are
 * not a qualified XFixes observer or evidence of protected Xlib allocation. */
enum stream_mode {
	STREAM_DIRECT, STREAM_INCR, STREAM_EMPTY, STREAM_MAX, STREAM_OVER,
	STREAM_BAD_LATER, STREAM_STALL, STREAM_FLOOD, STREAM_LOST, STREAM_DEATH
};
struct stream_owner { pid_t pid; int release; };
struct stream_context {
	Display *display, *reply_owner;
	Atom selection;
	Window replacement;
	uint64_t epoch, generation, now;
	size_t seen, calls, refuse, clocks, observations_after_write;
	size_t observations;
	bool stale, cancel, replace, fail_clock, expire_observer;
	struct content_payload_owner *clear;
};
static struct content_process_guard process_guard;
static Window test_stream_requestor;

static int owner_error(Display *display, XErrorEvent *error)
{
	(void)display;
	/* Only this child-owned conversion may race requestor retirement. */
	if (error->error_code == BadWindow &&
		error->resourceid == test_stream_requestor)
		return 0;
	fprintf(stderr, "Unexpected owner X error %u for %lu\n",
		error->error_code, error->resourceid);
	_exit(1);
}

static int64_t milliseconds(void)
{
	struct timespec now;

	assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static bool owner_event(Display *display, XEvent *event, int release)
{
	int64_t end = milliseconds() + 2000;
	struct pollfd fds[] = {
		{ConnectionNumber(display), POLLIN, 0}, {release, POLLIN, 0}
	};

	while (milliseconds() < end)
	{
		if (XPending(display))
		{
			XNextEvent(display, event);
			return true;
		}
		int64_t remaining = end - milliseconds();

		if (remaining <= 0)
			return false;
		int result = poll(fds, release < 0 ? 1 : 2, (int)remaining);

		assert(result >= 0 || errno == EINTR);
		if (result > 0 && release >= 0 && fds[1].revents)
			return false;
	}
	return false;
}

static bool owner_ack(Display *display, const XSelectionRequestEvent *request,
	int release)
{
	XEvent event;

	while (owner_event(display, &event, release))
	{
		if (event.type == DestroyNotify &&
			event.xdestroywindow.window == request->requestor)
			return false;
		if (event.type == PropertyNotify &&
			event.xproperty.window == request->requestor &&
			event.xproperty.atom == request->property &&
			event.xproperty.state == PropertyDelete)
			return true;
	}
	return false;
}

static void owner_reply(Display *display, XSelectionRequestEvent request,
	enum stream_mode mode, int release)
{
	unsigned char pattern[FSELECTION_TEXT_MAX_BYTES + 1];
	XEvent event = {.xselection = {.type = SelectionNotify,
		.requestor = request.requestor, .selection = request.selection,
		.target = request.target, .property = request.property,
		.time = request.time}};
	bool incr = mode == STREAM_INCR || mode == STREAM_BAD_LATER ||
		mode == STREAM_FLOOD;

	for (size_t i = 0; i < sizeof(pattern); i++)
		pattern[i] = (unsigned char)i;
	if (mode == STREAM_STALL)
		return;
	if (!incr)
	{
		int length = mode == STREAM_EMPTY ? 0 :
			mode == STREAM_MAX ? FSELECTION_TEXT_MAX_BYTES :
			mode == STREAM_OVER ? FSELECTION_TEXT_MAX_BYTES + 1 : 5;

		XChangeProperty(display, request.requestor, request.property,
			request.target, 8, PropModeReplace, pattern, length);
	}
	else
	{
		Atom type = XInternAtom(display, "INCR", False);
		unsigned long hint = 0; /* Never an allocation authority. */

		XSelectInput(display, request.requestor,
			PropertyChangeMask | StructureNotifyMask);
		XChangeProperty(display, request.requestor, request.property,
			type, 32, PropModeReplace, (unsigned char *)&hint, 1);
	}
	/* Header NewValue precedes SelectionNotify. */
	if (mode == STREAM_LOST)
		XSetSelectionOwner(display, request.selection, None,
			CurrentTime);
	XSync(display, False);
	assert(XSendEvent(display, request.requestor, False, 0, &event));
	XFlush(display);
	if (!incr || !owner_ack(display, &request, release))
		return;
	size_t offset = 0;
	unsigned count = mode == STREAM_FLOOD ? 4097 : 3;
	const int lengths[] = {3, 4097, 4093};

	for (unsigned i = 0; i < count; i++)
	{
		int length = mode == STREAM_FLOOD ? 1 : lengths[i];
		Atom type = mode == STREAM_BAD_LATER && i == 1 ?
			XA_INTEGER : request.target;

		XChangeProperty(display, request.requestor, request.property,
			type, 8, PropModeReplace, pattern + offset, length);
		XFlush(display);
		if (!owner_ack(display, &request, release))
			return;
		offset += (size_t)length;
	}
	XChangeProperty(display, request.requestor, request.property,
		request.target, 8, PropModeReplace, pattern, 0);
	XFlush(display);
}

static Display *owner_disconnect(Display *display,
	XSelectionRequestEvent request)
{
	const unsigned char original[] = {0, 1, 2, 3, 4};
	XEvent event = {.xselection = {.type = SelectionNotify,
		.requestor = request.requestor, .selection = request.selection,
		.target = request.target, .property = request.property,
		.time = request.time}};

	XChangeProperty(display, request.requestor, request.property,
		request.target, 8, PropModeReplace, original, sizeof(original));
	XSync(display, False);
	/* A distinct non-owner connection delivers the notification only after
	 * the original owner connection/window has disappeared. */
	Display *notifier = XOpenDisplay(NULL);

	assert(notifier);
	XCloseDisplay(display);
	int64_t end = milliseconds() + 1000;

	while (XGetSelectionOwner(notifier, request.selection) != None)
	{
		assert(milliseconds() < end);
		assert(poll(NULL, 0, 1) == 0);
	}
	XSendEvent(notifier, request.requestor, False, 0, &event);
	XFlush(notifier);
	return notifier;
}

static struct stream_owner start_stream_owner(Display *parent, Atom selection,
	enum stream_mode mode)
{
	int ready[2], release[2];

	assert(pipe(ready) == 0 && pipe(release) == 0);
	pid_t pid = fork();

	assert(pid >= 0);
	if (!pid)
	{
		close(ready[0]);
		close(release[1]);
		close(ConnectionNumber(parent));
		/* Also bounds parent assertion/cancellation. */
		alarm(3);
		Display *display = XOpenDisplay(NULL);

		assert(display);
		XSetErrorHandler(owner_error);
		Window window = XCreateSimpleWindow(display,
			DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);

		XSetSelectionOwner(display, selection, window, CurrentTime);
		XSync(display, False);
		assert(XGetSelectionOwner(display, selection) == window);
		assert(write(ready[1], "r", 1) == 1);
		close(ready[1]);
		XEvent event;

		while (owner_event(display, &event, release[0]))
			if (event.type == SelectionRequest)
			{
				test_stream_requestor =
					event.xselectionrequest.requestor;
				if (mode == STREAM_DEATH)
					display = owner_disconnect(display,
						event.xselectionrequest);
				else
					owner_reply(display,
						event.xselectionrequest, mode, release[0]);
				break;
			}
		/* Keep the source alive through final fences. */
		char byte;

		assert(read(release[0], &byte, 1) == 0);
		close(release[0]);
		XCloseDisplay(display);
		_exit(0);
	}
	close(ready[1]);
	close(release[0]);
	char byte;

	assert(read(ready[0], &byte, 1) == 1);
	close(ready[0]);
	return (struct stream_owner){pid, release[1]};
}

static void finish_stream_owner(struct stream_owner owner)
{
	int status;

	close(owner.release);
	assert(waitpid(owner.pid, &status, 0) == owner.pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static bool stream_current(Display *display, Atom selection,
	FSelectionSource source, void *context)
{
	struct stream_context *state = context;

	assert(source.owner != None);
	assert(display == state->display && selection == state->selection);
	if (state->reply_owner && ++state->observations == 4)
	{
		XEvent event;

		/* No service callback: withhold the reply until the first empty
		 * transfer scan, then queue it in the pre-poll source fence. */
		assert(!state->calls);
		XSync(state->reply_owner, False);
		assert(XCheckTypedEvent(state->reply_owner, SelectionRequest,
			&event));
		owner_reply(state->reply_owner, event.xselectionrequest,
			STREAM_DIRECT, -1);
		XSync(state->reply_owner, False);
		XSync(display, False);
	}

	/* One direct native write samples clock twice (preflight and write).
	 * Three observer calls remain: post-consume, requestor-retired
	 * completion, and adapter publication. Advance only the last observer,
	 * so a clock sampled BEFORE that callback would incorrectly publish. */
	if (state->expire_observer && state->clocks >= 2 &&
		++state->observations_after_write == 3)
		state->now = 1000;
	return source.epoch == state->epoch &&
		source.generation == state->generation;
}

static bool stream_consume(const unsigned char *bytes, size_t offset,
	size_t length, void *context)
{
	struct stream_context *state = context;

	assert(offset == state->seen && length <= 4096);
	assert(length ? bytes != NULL : bytes == NULL);
	for (size_t i = 0; i < length; i++)
		assert(bytes[i] == (unsigned char)(offset + i));
	state->calls++;
	if (state->calls == state->refuse)
		return false;
	state->seen += length;
	if (state->stale)
		state->generation++;
	return true;
}

static bool stream_service(void *context)
{
	struct stream_context *state = context;

	if (state->clear)
	{
		struct content_payload_error error;

		assert(content_payload_clear(state->clear, &error) ==
			CONTENT_PAYLOAD_OK);
		state->clear = NULL;
		state->cancel = true;
	}
	if (state->replace)
	{
		XSetSelectionOwner(state->display, state->selection,
			state->replacement, CurrentTime);
		XSync(state->display, False);
		state->replace = false;
	}
	return !state->cancel;
}

static bool stream_clock(void *context, uint64_t *now_ms)
{
	struct stream_context *state = context;

	state->clocks++;
	*now_ms = state->now;
	return !(state->fail_clock && state->clocks == 3);
}

static void sink_tests(Display *display, Atom selection, Atom property,
	Window replacement)
{
	for (unsigned scenario = 0; scenario < 16; scenario++)
	{
		enum stream_mode mode = scenario == 1 ? STREAM_INCR :
			scenario == 2 ? STREAM_EMPTY :
			scenario == 3 ? STREAM_MAX :
			scenario == 4 ? STREAM_OVER :
			scenario == 5 ? STREAM_BAD_LATER :
			scenario >= 9 && scenario <= 10 ? STREAM_STALL :
			scenario == 11 ? STREAM_FLOOD :
			scenario == 12 ? STREAM_LOST :
			scenario == 13 ? STREAM_STALL :
			scenario == 14 ? STREAM_DEATH : STREAM_DIRECT;
		struct stream_owner owner = start_stream_owner(display,
			selection,
			mode);
		struct stream_context state = {.display = display,
			.selection = selection, .replacement = replacement,
			.epoch = 100 + scenario, .generation = 17,
			.refuse = scenario == 6 ? 1 : 0,
			.stale = scenario == 7, .cancel = scenario == 9,
			.replace = scenario == 10};
		FSelectionSink sink = {
			.source = {.owner = XGetSelectionOwner(display,
				selection),
				.epoch = state.epoch, .generation = 17},
			.maximum = mode == STREAM_EMPTY ? 0 : 32768,
			.current = stream_current, .consume = stream_consume,
			.context = &state};
		FSelectionReceipt receipt = {.length = 99, .type = XA_ATOM};
		if (scenario == 8)
			state.generation++;
		if (scenario == 15)
			state.epoch++;
		FSelectionStatus status = FSelectionRequestExactTo(display,
			selection, XA_STRING, property, CurrentTime,
			scenario == 13 ? 200 : 1000,
			stream_service, &state, &sink, &receipt);

		if (scenario < 4)
		{
			size_t expected = scenario == 1 ? 8193 :
				scenario == 2 ? 0 : scenario == 3 ? 32768 : 5;

			assert(status == FSELECTION_OK &&
				receipt.length == expected);
			assert(receipt.type == XA_STRING &&
				state.seen == expected);
			assert(state.calls);
		}
		else
		{
			assert(status != FSELECTION_OK && !receipt.length &&
				receipt.type == None);
			if (scenario == 4 || scenario == 8 || scenario == 15)
				assert(!state.calls);
			if (scenario == 5)
				assert(state.seen == 3 &&
					status == FSELECTION_INVALID);
			if (scenario == 6)
				assert(status == FSELECTION_CONSUMER_FAILED);
			if (scenario == 7 || scenario == 8 || scenario == 10 ||
				scenario == 15)
				assert(status == FSELECTION_STALE_SOURCE);
			if (scenario == 9)
				assert(status == FSELECTION_CANCELLED);
			if (scenario == 11)
				assert(state.seen <= 4096 &&
					(status == FSELECTION_TOO_LARGE ||
					 status == FSELECTION_TIMED_OUT));
			if (scenario == 12 || scenario == 14)
				assert(status == FSELECTION_STALE_SOURCE &&
					!state.calls);
			if (scenario == 13)
				assert(status == FSELECTION_TIMED_OUT &&
					!state.calls);
		}
		finish_stream_owner(owner);
	}
}

static bool verify_original(const unsigned char *bytes, size_t length,
	void *context)
{
	size_t *offset = context;

	for (size_t i = 0; i < length; i++)
		assert(bytes[i] == (unsigned char)(*offset + i));
	*offset += length;
	return true;
}

static void queued_reply_test(Display *display, Atom selection, Atom property)
{
	Display *owner = XOpenDisplay(NULL);

	assert(owner);
	Window window = XCreateSimpleWindow(owner, DefaultRootWindow(owner),
		0, 0, 1, 1, 0, 0, 0);

	XSetSelectionOwner(owner, selection, window, CurrentTime);
	XSync(owner, False);
	struct stream_context state = {.display = display, .reply_owner = owner,
		.selection = selection, .epoch = 91, .generation = 19};
	FSelectionSink sink = {.source = {.owner = window, .epoch = 91,
		.generation = 19}, .maximum = 5, .current = stream_current,
		.consume = stream_consume, .context = &state};
	FSelectionReceipt receipt;

	assert(FSelectionRequestExactTo(display, selection, XA_STRING, property,
		CurrentTime, 1000, NULL, NULL, &sink, &receipt) == FSELECTION_OK);
	assert(state.observations >= 4 && state.calls == 1 && state.seen == 5);
	assert(receipt.length == 5 && receipt.type == XA_STRING);
	XDestroyWindow(owner, window);
	XCloseDisplay(owner);
}

static void adapter_tests(Display *display, Atom selection, Atom property)
{
	for (unsigned scenario = 0; scenario < 9; scenario++)
	{
		struct content_payload_owner payload;
		struct content_item items[2];
		struct content_lease leases[4];
		struct content_payload_slot slots[2];
		struct content_payload_error error;
		struct content_ref ref;
		struct content_limits limits = {
			.items = 2, .representations = 2, .per_item = 1,
			.leases = 4, .work = 2, .total = {65536, 0, 0},
			.per_representation = {32768, 0, 0},
			.storage = {1048576, 524288},
			.per_item_storage = {524288, 262144},
			.lifetime_ms = 1000
		};
		struct content_candidate candidate = {
			.deadline_ms = 1000, .count = 1,
			.reps = {{.id = 11, .kind = CONTENT_TEXT,
				.bytes = {.encoded = scenario == 3 ||
					scenario == 8 ? 3 : 8193}}}
		};
		assert(content_payload_init(&payload, &process_guard,
			1000 + scenario, &limits, items, leases, slots, 100,
			&error) == CONTENT_PAYLOAD_OK);
		assert(content_payload_set_admission(&payload, true, false,
			&error) == CONTENT_PAYLOAD_OK);
		assert(content_payload_reserve(&payload, &candidate, true, NULL,
			100, &ref, &error) == CONTENT_PAYLOAD_OK);
		enum stream_mode mode = scenario == 1 || scenario == 8 ?
			STREAM_INCR :
			scenario == 2 ? STREAM_EMPTY :
			scenario == 7 ? STREAM_STALL : STREAM_DIRECT;
		struct stream_owner owner = start_stream_owner(display,
			selection,
			mode);
		struct stream_context state = {.display = display,
			.selection = selection, .now = 995,
			.epoch = 2000 + scenario,
			.generation = 29,
			.fail_clock = scenario == 4,
			.expire_observer = scenario == 5,
			.clear = scenario == 7 ? &payload : NULL};
		struct content_selection_options options = {.display = display,
			.selection = selection, .target = XA_STRING,
			.property = property, .timeout_ms = 1000,
			.source = {.owner = XGetSelectionOwner(display,
				selection),
				.epoch = state.epoch, .generation = 29},
			.current = stream_current, .clock = stream_clock,
			.context = &state};
		if (scenario == 7)
			options.service = stream_service;
		if (scenario == 6)
			options.hint = CONTENT_HINT_SENSITIVE;
		struct content_selection_result result;
		enum content_selection_status status =
			content_selection_receive(
			&payload, ref, 11, &options, &result);

		assert(result.producer_taken);
		if (scenario < 3)
		{
			size_t expected = scenario == 1 ? 8193 :
				scenario == 2 ? 0 : 5;
			size_t offset = 0;

			if (status != CONTENT_SELECTION_OK ||
				result.length != expected ||
				result.type != XA_STRING)
				fprintf(stderr, "adapter scenario=%u status=%u "
					"transfer=%u payload=%u "
					"policy=%u op=%u "
					"errno=%d length=%zu "
					"type=%lu clocks=%zu\n",
					scenario, status, result.transfer,
					result.payload,
					result.error.policy,
					result.error.operation,
					result.error.number, result.length,
					result.type,
					state.clocks);
			assert(status == CONTENT_SELECTION_OK &&
				result.length == expected &&
				result.type == XA_STRING);
			while (offset < expected)
			{
				size_t amount = expected - offset;

				if (amount > CONTENT_PAYLOAD_CHUNK_MAX)
					amount = CONTENT_PAYLOAD_CHUNK_MAX;
				assert(content_payload_with_read(&payload,
					ref, 11,
					CONTENT_PAYLOAD_ENCODED, offset, amount,
					verify_original, &offset, 995,
					&error) ==
						CONTENT_PAYLOAD_OK);
			}
			/* LIVE preflight cannot transfer completion or retire. */
			struct content_selection_result again;

			assert(content_selection_receive(&payload, ref, 11,
				&options, &again) == CONTENT_SELECTION_PAYLOAD);
			assert(!again.producer_taken &&
				items[0].phase == CONTENT_LIVE);
		}
		else
		{
			assert(status != CONTENT_SELECTION_OK &&
				!result.length &&
				result.type == None &&
				items[0].phase == CONTENT_RETIRED);
			assert(result.retirement == CONTENT_PAYLOAD_OK);
			assert(result.retirement_error.cleanup_pending);
			assert(result.error.cleanup_pending &&
				payload.policy.usage.storage.mapped &&
				payload.policy.usage.storage.locked);
			if (scenario == 4)
				assert(status == CONTENT_SELECTION_CLOCK);
			if (scenario == 5)
				assert(state.now == 1000 &&
					result.error.policy == CONTENT_EXPIRED);
			if (scenario == 6)
				assert(state.clocks == 1 &&
					result.error.policy ==
						CONTENT_EXCLUDED);
			if (scenario == 8)
				assert(slots[0].regions[0].written.encoded ==
					3);
		}
		finish_stream_owner(owner);
		assert(content_payload_clear(&payload, &error) ==
			CONTENT_PAYLOAD_OK);
		for (unsigned i = 0; payload.policy.usage.items && i < 256; i++)
		{
			bool progress;

			assert(content_payload_cleanup_step(&payload, &progress,
				&error) == CONTENT_PAYLOAD_OK && progress);
		}
		assert(!payload.policy.usage.items &&
			!payload.policy.usage.work &&
			!payload.policy.usage.leases &&
			!payload.policy.usage.storage.mapped &&
			!payload.policy.usage.storage.locked);
	}
}

int main(void)
{
	struct content_payload_error error;
	struct rlimit memlock;
	long page = sysconf(_SC_PAGESIZE);

	if (page <= 0 || page > 65536 ||
		content_payload_process_prepare(&process_guard, &error) !=
			CONTENT_PAYLOAD_OK ||
		getrlimit(RLIMIT_MEMLOCK, &memlock) < 0 ||
		memlock.rlim_cur < (rlim_t)(4 * page))
		return 77;
	Display *display = XOpenDisplay(NULL);

	assert(display);
	Atom selection = XInternAtom(display,
		"FVWM_CONTENT_SELECTION_TEST", False);
	Atom property = XInternAtom(display,
		"FVWM_CONTENT_SELECTION_DATA", False);
	Window replacement = XCreateSimpleWindow(display,
		DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
	XEvent marker = {.xexpose = {.type = Expose,
		.window = replacement, .count = 42}};

	XPutBackEvent(display, &marker);
	marker.xexpose.count = 41;
	XPutBackEvent(display, &marker);
	FSelectionReceipt invalid = {.length = 99, .type = XA_ATOM};

	assert(FSelectionRequestExactTo(NULL, selection, XA_STRING, property,
		CurrentTime, 1000, NULL, NULL, NULL, &invalid) ==
			FSELECTION_INVALID);
	assert(!invalid.length && invalid.type == None);

	queued_reply_test(display, selection, property);
	sink_tests(display, selection, property, replacement);
	adapter_tests(display, selection, property);
	assert(XCheckTypedEvent(display, Expose, &marker) &&
		marker.xexpose.count == 41);
	assert(XCheckTypedEvent(display, Expose, &marker) &&
		marker.xexpose.count == 42);
	XDestroyWindow(display, replacement);
	XCloseDisplay(display);
	puts("borrowed selection bytes, fences and native producer lifecycle "
		"passed");
	return 0;
}
