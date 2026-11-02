/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "bin/clipboard_policy.h"
#include "libs/content_selection.h"
#include "libs/selection_observer.h"
#include "libs/safemalloc.h"
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

/* Only this isolated process ever owns payload memory. No selection ownership,
 * persistence, previews, exports or decoded/derived copies. The native owner
 * stays on one thread; a transfer services only bounded parent controls and
 * its exact observer, leaving requestor events to FSelection. */
struct clipboard_runtime {
	struct clipboard_config config;
	struct content_payload_owner payload;
	struct selection_observer observer;
	struct clipboard_request request;
	struct content_payload_comparison comparison;
	struct content_ref captured;
	Display *display;
	Atom selection, target, sensitive, property;
	uint64_t sequence, window_deadline, transfer_deadline;
	size_t compare_index;
	enum clipboard_mode mode;
	bool pending, input_ready, clear_barrier, stop, failed;
	char input[CLIPBOARD_FRAME_MAX], output[CLIPBOARD_REPLY_MAX];
	size_t input_used, output_used, output_sent;
};
static volatile sig_atomic_t interrupted;
static bool x_failed;

static void stop_signal(int number)
{
	(void)number;
	interrupted = 1;
}

static int x_error(Display *display, XErrorEvent *event)
{
	(void)display;
	(void)event;
	x_failed = true;
	return 0;
}

static int x_io_error(Display *display)
{
	(void)display;
	/* Xlib cannot return safely after connection loss. Process exit releases
	 * mappings, but does not establish a completed native wipe. */
	fputs("fvwm-clipboard-capture: X connection lost; cleanup unconfirmed\n",
		stderr);
	_exit(1);
}

static bool clock_ms(void *context, uint64_t *output)
{
	struct timespec now;

	(void)context;
	*output = 0;
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
		(uint64_t)now.tv_sec > (UINT64_MAX - 999) / 1000)
		return false;
	*output = (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
	return true;
}

static bool nonblocking(int descriptor)
{
	int flags = fcntl(descriptor, F_GETFL);

	return flags >= 0 && fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == 0;
}

static bool capture_clock(void *context, uint64_t *output)
{
	struct clipboard_runtime *runtime = context;

	return clock_ms(NULL, output) && *output < runtime->transfer_deadline;
}

static void read_control(struct clipboard_runtime *runtime)
{
	struct pollfd parent = {STDIN_FILENO, 0, 0};
	int ready = poll(&parent, 1, 0);

	/* Observe EOF even behind a queued reply or clear barrier. Input is
	 * bounded but may be deliberately backpressured during those states. */
	if ((ready < 0 && errno != EINTR) ||
		(parent.revents & (POLLHUP | POLLERR | POLLNVAL)))
	{
		runtime->failed |= ready < 0 || runtime->input_used != 0 ||
			(parent.revents & (POLLERR | POLLNVAL)) != 0;
		runtime->stop = true;
		return;
	}
	if (runtime->input_ready || runtime->output_used ||
		runtime->clear_barrier || runtime->stop)
		return;
	for (size_t i = 0; i < CLIPBOARD_FRAME_MAX; i++)
	{
		char byte;
		ssize_t count = read(STDIN_FILENO, &byte, 1);

		if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return;
		if (count < 0 && errno == EINTR)
			return;
		if (count <= 0)
		{
			runtime->failed |= count < 0 || runtime->input_used != 0;
			runtime->stop = true;
			return;
		}
		if (byte == '\n')
		{
			if (!clipboard_request_parse(runtime->input,
				runtime->input_used, runtime->config.epoch,
				runtime->sequence, &runtime->request))
			{
				runtime->stop = runtime->failed = true;
				return;
			}
			runtime->input_used = 0;
			runtime->input_ready = true;
			return;
		}
		if (runtime->input_used == sizeof(runtime->input))
		{
			runtime->stop = runtime->failed = true;
			return;
		}
		runtime->input[runtime->input_used++] = byte;
	}
}

static bool service(void *context)
{
	struct clipboard_runtime *runtime = context;
	uint64_t now = 0;

	read_control(runtime);
	/* Even metadata queries interrupt a transfer. A pause/clear/quit reply
	 * cannot precede its producer retirement. The outer turn applies the
	 * one retained frame after the transfer has unwound. */
	return !runtime->stop && !runtime->input_ready && !interrupted &&
		!x_failed && runtime->mode == CLIPBOARD_UNLOCKED &&
		capture_clock(runtime, &now);
}

static bool current(Display *display, Atom selection, FSelectionSource source,
	void *context)
{
	struct clipboard_runtime *runtime = context;
	uint64_t now = 0;

	return !runtime->stop && !runtime->input_ready && !interrupted &&
		!x_failed && runtime->mode == CLIPBOARD_UNLOCKED &&
		capture_clock(runtime, &now) &&
		selection_observer_current(display, selection, source,
			&runtime->observer) && capture_clock(runtime, &now);
}

static enum clipboard_result payload_result(enum content_payload_result result,
	struct content_payload_error error)
{
	if (result == CONTENT_PAYLOAD_OK)
		return CLIPBOARD_OK;
	if (result == CONTENT_PAYLOAD_FATAL)
		return CLIPBOARD_FATAL;
	if (error.policy == CONTENT_STALE || error.policy == CONTENT_EXPIRED)
		return CLIPBOARD_STALE;
	if (error.policy == CONTENT_FULL)
		return CLIPBOARD_FULL;
	return CLIPBOARD_REFUSED;
}

static void write_reply(struct clipboard_runtime *runtime,
	enum clipboard_result result, struct content_ref ref,
	const struct content_candidate *candidate, bool pinned)
{
	const struct content_usage *usage = &runtime->payload.policy.usage;
	bool visible = runtime->mode == CLIPBOARD_UNLOCKED;
	int count = snprintf(runtime->output, sizeof(runtime->output),
		"1 %" PRIu64 " %u %" PRIu64 " %" PRIu64 " %" PRIu64
		" %u %zu %" PRIu64 " %u %u %zu %zu %zu\n",
		runtime->request.sequence, (unsigned)result,
		runtime->config.epoch, ref.item, ref.generation,
		candidate ? candidate->reps[0].id : 0,
		candidate ? candidate->reps[0].bytes.encoded : 0,
		candidate ? candidate->deadline_ms : 0, pinned ? 1U : 0U,
		(unsigned)runtime->mode, visible ? usage->items : 0,
		visible ? usage->storage.mapped : 0,
		visible ? usage->storage.locked : 0);

	if (count < 0 || (size_t)count >= sizeof(runtime->output))
	{
		runtime->stop = runtime->failed = true;
		return;
	}
	runtime->output_used = (size_t)count;
	runtime->output_sent = 0;
}

static void flush_output(struct clipboard_runtime *runtime)
{
	if (!runtime->output_used)
		return;
	ssize_t count = write(STDOUT_FILENO,
		runtime->output + runtime->output_sent,
		runtime->output_used - runtime->output_sent);

	if (count < 0 && (errno == EINTR || errno == EAGAIN ||
		errno == EWOULDBLOCK))
		return;
	if (count <= 0)
	{
		runtime->stop = runtime->failed = true;
		return;
	}
	runtime->output_sent += (size_t)count;
	if (runtime->output_sent == runtime->output_used)
		runtime->output_used = runtime->output_sent = 0;
}

static void apply_control(struct clipboard_runtime *runtime, uint64_t now)
{
	struct clipboard_request request = runtime->request;
	struct content_payload_error error = {0};
	struct content_candidate candidate = {0};
	struct content_ref ref = {0};
	enum clipboard_result result = CLIPBOARD_OK;
	bool pinned = false, described = false;

	runtime->input_ready = false;
	runtime->sequence = request.sequence;
	if (request.verb == CLIPBOARD_STATE)
	{
		runtime->mode = (enum clipboard_mode)request.value;
		content_payload_set_admission(&runtime->payload, true,
			runtime->mode != CLIPBOARD_UNLOCKED, &error);
		/* Any state transition requires a new ordered snapshot. */
		runtime->pending = true;
		runtime->comparison = (struct content_payload_comparison){0};
		runtime->captured = (struct content_ref){0};
	}
	else if (request.verb == CLIPBOARD_QUIT)
		runtime->stop = true;
	else if (request.verb == CLIPBOARD_CLEAR)
	{
		enum content_payload_result status = content_payload_clear(
			&runtime->payload, &error);

		result = payload_result(status, error);
		runtime->comparison = (struct content_payload_comparison){0};
		runtime->captured = (struct content_ref){0};
		runtime->window_deadline = 0;
		/* Clear does not recapture the unchanged source. */
		runtime->pending = false;
		if (result == CLIPBOARD_OK)
		{
			/* Establish a clean ordered source baseline before replying.
			 * Notifications preceding CLEAR cannot schedule recapture. */
			runtime->clear_barrier = true;
			return;
		}
	}
	else if (request.verb != CLIPBOARD_STATUS &&
		runtime->mode != CLIPBOARD_UNLOCKED)
		result = CLIPBOARD_BLOCKED;
	else if (request.verb == CLIPBOARD_PIN)
	{
		enum content_payload_result status = content_payload_pin(
			&runtime->payload, request.ref, request.value != 0, now, &error);

		result = payload_result(status, error);
	}
	else if (request.verb == CLIPBOARD_REMOVE)
	{
		/* Retirement itself accepts retired refs; validate live identity
		 * first so repeated/stale UI actions cannot pretend to succeed. */
		enum content_payload_result status = content_payload_describe(
			&runtime->payload, request.ref, now, &candidate,
			&pinned, &error);

		if (status == CONTENT_PAYLOAD_OK)
			status = content_payload_retire(&runtime->payload,
				request.ref, &error);
		result = payload_result(status, error);
	}
	else if (request.verb == CLIPBOARD_LIST)
	{
		result = CLIPBOARD_EMPTY;
		if (request.value < runtime->config.items)
		{
			const struct content_item *item =
				&runtime->payload.policy.items[request.value];

			if (item->phase == CONTENT_LIVE &&
				content_payload_describe(&runtime->payload,
					item->ref, now, &candidate, &pinned,
					&error) == CONTENT_PAYLOAD_OK)
			{
				ref = item->ref;
				result = CLIPBOARD_OK;
				described = true;
			}
		}
	}
	if (runtime->payload.fatal)
	{
		runtime->failed = runtime->stop = true;
		result = CLIPBOARD_FATAL;
	}
	write_reply(runtime, result, ref, described ? &candidate : NULL,
		described && pinned);
}

static void complete_clear(struct clipboard_runtime *runtime)
{
	FSelectionSource source = {0};
	enum selection_source_result result = selection_observer_refresh(
		&runtime->observer, &source);

	if (result == SELECTION_SOURCE_RETRY)
		return;
	if (result != SELECTION_SOURCE_OK && result != SELECTION_SOURCE_EMPTY)
	{
		runtime->stop = runtime->failed = true;
		return;
	}
	runtime->clear_barrier = false;
	runtime->pending = false;
	write_reply(runtime, CLIPBOARD_OK, (struct content_ref){0}, NULL, false);
}

static void observe_events(struct clipboard_runtime *runtime)
{
	for (unsigned i = 0; i < SELECTION_OBSERVER_EVENT_MAX &&
		XEventsQueued(runtime->display, QueuedAlready) > 0; i++)
	{
		XEvent event;
		bool consumed = false;

		XNextEvent(runtime->display, &event);
		selection_observer_event(&runtime->observer, &event, &consumed);
		if (consumed)
			runtime->pending = true;
		/* This private display has no other consumer. Transfer events
		 * are handled only inside its synchronous entered request. */
	}
}

static int remaining(uint64_t deadline, uint64_t now)
{
	if (now >= deadline)
		return 0;
	return (int)(deadline - now);
}

static void capture(struct clipboard_runtime *runtime, uint64_t now)
{
	FSelectionSource source = {0};
	FSelectionResult targets = {0};
	struct content_payload_error error = {0};
	struct content_ref ref = {0};
	struct content_selection_result received = {0};
	struct content_candidate candidate = {.hint = CONTENT_HINT_ABSENT,
		.count = 1, .reps = {{.id = 1, .kind = CONTENT_TEXT,
			.bytes = {.encoded = runtime->config.bytes}}}};
	uint64_t end;
	bool supported = false;

	if (now > UINT64_MAX - FSELECTION_TIMEOUT_MS)
	{
		runtime->stop = runtime->failed = true;
		return;
	}
	end = now + FSELECTION_TIMEOUT_MS;
	runtime->transfer_deadline = end;
	enum selection_source_result observed = selection_observer_refresh(
		&runtime->observer, &source);

	if (observed == SELECTION_SOURCE_RETRY)
		return;
	runtime->pending = false;
	if (observed == SELECTION_SOURCE_EMPTY)
		return;
	if (observed != SELECTION_SOURCE_OK)
	{
		runtime->stop = runtime->failed = true;
		return;
	}
	if (!capture_clock(runtime, &now))
		return;
	FSelectionStatus status = FSelectionRequestTargets(runtime->display,
		runtime->selection, runtime->property, CurrentTime,
		remaining(end, now), source, current, service, runtime, &targets);

	if (status == FSELECTION_OK)
		candidate.hint = clipboard_targets((const uint32_t *)targets.data,
			targets.length / 4, (uint32_t)runtime->target,
			(uint32_t)runtime->sensitive, &supported);
	FSelectionResultFree(&targets);
	if (runtime->observer.policy.phase != SELECTION_SOURCE_PHASE_READY ||
		status == FSELECTION_CANCELLED || runtime->input_ready)
		runtime->pending = true;
	/* Unavailable/malformed discovery never guesses an unadvertised text
	 * target. The recognized marker is rejected before requesting bytes. */
	if (status != FSELECTION_OK || !supported ||
		candidate.hint != CONTENT_HINT_ABSENT || !service(runtime) ||
		!capture_clock(runtime, &now))
		return;
	if (!runtime->window_deadline || now >= runtime->window_deadline)
	{
		if (now > UINT64_MAX - runtime->config.lifetime_ms)
		{
			runtime->stop = runtime->failed = true;
			return;
		}
		runtime->window_deadline = now + runtime->config.lifetime_ms;
	}
	/* One caller-sized retention window. All its items share a deadline,
	 * allowing strict equality dedup without extending any older lifetime.
	 * Pinning never extends this window. This is not an installed default. */
	candidate.deadline_ms = runtime->window_deadline;
	if (content_payload_reserve(&runtime->payload, &candidate, true, NULL,
		now, &ref, &error) != CONTENT_PAYLOAD_OK)
		return;
	/* Allocation/locking and X queries are not preemptible. Never let their
	 * elapsed time grant a fresh conversion budget or late publication. */
	if (!capture_clock(runtime, &now))
	{
		content_payload_retire(&runtime->payload, ref, &error);
		return;
	}
	struct content_selection_options options = {.display = runtime->display,
		.selection = runtime->selection, .target = runtime->target,
		.property = runtime->property, .time = CurrentTime,
		.timeout_ms = remaining(end, now), .source = source,
		.current = current, .service = service, .clock = capture_clock,
		.context = runtime, .hint = candidate.hint};

	if (content_selection_receive(&runtime->payload, ref, 1, &options,
		&received) == CONTENT_SELECTION_OK)
	{
		runtime->captured = ref;
		runtime->compare_index = 0;
	}
	else if (!received.producer_taken)
		content_payload_retire(&runtime->payload, ref, &error);
	/* A source change during either conversion schedules only the latest
	 * snapshot, never a FIFO of retained owner/payload generations. */
	if (runtime->observer.policy.phase != SELECTION_SOURCE_PHASE_READY ||
		received.transfer == FSELECTION_CANCELLED || runtime->input_ready)
		runtime->pending = true;
}

static void compare_step(struct clipboard_runtime *runtime, uint64_t now)
{
	struct content_payload_error error = {0};
	enum content_payload_comparison_outcome outcome;

	if (runtime->comparison.state != CONTENT_PAYLOAD_COMPARE_EMPTY &&
		runtime->comparison.state != CONTENT_PAYLOAD_COMPARE_ACTIVE)
		runtime->comparison = (struct content_payload_comparison){0};
	if (runtime->comparison.state == CONTENT_PAYLOAD_COMPARE_ACTIVE)
	{
		if (content_payload_compare_step(&runtime->payload,
			&runtime->comparison, &outcome, now, &error) !=
			CONTENT_PAYLOAD_OK || outcome != CONTENT_PAYLOAD_COMPARE_MORE)
		{
			if (runtime->comparison.state == CONTENT_PAYLOAD_COMPARE_EQUAL)
			{
				content_payload_dedup_commit(&runtime->payload,
					&runtime->comparison, now, &error);
				runtime->captured = (struct content_ref){0};
			}
			runtime->comparison = (struct content_payload_comparison){0};
		}
		return;
	}
	if (!runtime->captured.item)
		return;
	if (runtime->compare_index >= runtime->config.items)
	{
		runtime->captured = (struct content_ref){0};
		return;
	}
	const struct content_item *item =
		&runtime->payload.policy.items[runtime->compare_index++];

	if (item->phase == CONTENT_LIVE &&
		item->ref.item != runtime->captured.item)
		content_payload_compare_begin(&runtime->payload, item->ref,
			runtime->captured, &runtime->comparison, now, &error);
}

static bool maintain(struct clipboard_runtime *runtime, uint64_t now)
{
	struct content_payload_error error = {0};
	bool progress = false;

	content_payload_expire(&runtime->payload, now, &error);
	enum content_payload_result result = content_payload_cleanup_step(
		&runtime->payload, &progress, &error);

	if (result != CONTENT_PAYLOAD_OK || runtime->payload.fatal)
	{
		fputs("fvwm-clipboard-capture: native maintenance failed\n", stderr);
		runtime->stop = runtime->failed = true;
		return false;
	}
	return progress;
}

int main(int argc, char **argv)
{
	struct clipboard_runtime runtime = {0};
	struct content_process_guard guard = {0};
	struct content_payload_error error = {0};
	struct content_storage per_item = {0};
	uint64_t now = 0, cleanup_end = 0;
	long page;

	if (argc != 9 || strcmp(argv[1], "--serve") ||
		!clipboard_configure((const char *const *)&argv[2], &runtime.config))
		return 64;
	if (!clock_ms(NULL, &now) || !nonblocking(STDIN_FILENO) ||
		!nonblocking(STDOUT_FILENO) ||
		content_payload_process_prepare(&guard, &error) != CONTENT_PAYLOAD_OK)
		return 1;
	page = sysconf(_SC_PAGESIZE);
	if (page <= 0 || !content_storage_charge(runtime.config.bytes,
		(size_t)page, 2, 0, &per_item))
		return 1;
	struct content_limits limits = {.items = runtime.config.items,
		.representations = runtime.config.items, .per_item = 1, .leases = 2,
		.work = 1, .total = {.encoded = runtime.config.total},
		.per_representation = {.encoded = runtime.config.bytes},
		.storage = {runtime.config.mapped, runtime.config.locked},
		.per_item_storage = per_item,
		.lifetime_ms = runtime.config.lifetime_ms};
	struct content_item *items = fxcalloc(limits.items, sizeof(*items));
	struct content_lease leases[2] = {0};
	struct content_payload_slot *slots = fxcalloc(limits.items, sizeof(*slots));

	if (content_payload_init(&runtime.payload, &guard, runtime.config.epoch,
		&limits, items, leases, slots, now, &error) != CONTENT_PAYLOAD_OK)
		return 1;
	/* Fresh owners and every missing/uncertain lock observation block. */
	content_payload_set_admission(&runtime.payload, true, true, &error);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, stop_signal);
	signal(SIGINT, stop_signal);
	XSetErrorHandler(x_error);
	XSetIOErrorHandler(x_io_error);
	runtime.display = XOpenDisplay(NULL);
	if (!runtime.display)
		return 1;
	runtime.selection = XInternAtom(runtime.display, "CLIPBOARD", False);
	runtime.target = XInternAtom(runtime.display, "UTF8_STRING", False);
	runtime.sensitive = XInternAtom(runtime.display,
		"x-kde-passwordManagerHint", False);
	runtime.property = XInternAtom(runtime.display,
		"_FVWM_CLIPBOARD_CAPTURE", False);
	FSelectionSource initial = {0};
	enum selection_source_result observed = selection_observer_open(
		&runtime.observer, runtime.display, runtime.selection,
		runtime.config.epoch, &initial);

	if (observed != SELECTION_SOURCE_OK &&
		observed != SELECTION_SOURCE_EMPTY && observed != SELECTION_SOURCE_RETRY)
		runtime.failed = runtime.stop = true;
	runtime.pending = true;
	while (!runtime.stop && !interrupted && !x_failed)
	{
		struct pollfd descriptors[] = {
			{STDIN_FILENO, runtime.output_used ? 0 : POLLIN, 0},
			{STDOUT_FILENO, runtime.output_used ? POLLOUT : 0, 0},
			{ConnectionNumber(runtime.display), POLLIN, 0}
		};
		if (!clock_ms(NULL, &now))
		{
			runtime.failed = true;
			break;
		}
		bool progress = maintain(&runtime, now);

		read_control(&runtime);
		if (runtime.input_ready)
			apply_control(&runtime, now);
		flush_output(&runtime);
		if (runtime.stop || runtime.payload.fatal)
			break;
		observe_events(&runtime);
		if (runtime.clear_barrier)
			complete_clear(&runtime);
		if (!runtime.clear_barrier && runtime.mode == CLIPBOARD_UNLOCKED)
		{
			compare_step(&runtime, now);
			if (runtime.pending && !runtime.captured.item &&
				runtime.comparison.state != CONTENT_PAYLOAD_COMPARE_ACTIVE)
				capture(&runtime, now);
		}
		if (XEventsQueued(runtime.display, QueuedAlready) || progress ||
			(runtime.mode == CLIPBOARD_UNLOCKED &&
			 (runtime.pending || runtime.captured.item)))
			continue;
		int ready = poll(descriptors, 3, 20);

		if (ready < 0 && errno == EINTR)
			continue;
		if (ready < 0 || descriptors[0].revents &
			(POLLERR | POLLHUP | POLLNVAL) ||
			descriptors[1].revents & (POLLERR | POLLHUP | POLLNVAL) ||
			descriptors[2].revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			runtime.failed |= ready < 0 || runtime.input_used != 0 ||
				(descriptors[1].revents &
				 (POLLERR | POLLHUP | POLLNVAL)) != 0 ||
				(descriptors[2].revents &
				 (POLLERR | POLLHUP | POLLNVAL)) != 0;
			break;
		}
		if (descriptors[2].revents & POLLIN)
			XEventsQueued(runtime.display, QueuedAfterReading);
	}
	content_payload_set_admission(&runtime.payload, false, true, &error);
	content_payload_clear(&runtime.payload, &error);
	if (clock_ms(NULL, &now) && now <= UINT64_MAX - 2000)
		cleanup_end = now + 2000;
	while (runtime.payload.policy.usage.items && cleanup_end && now < cleanup_end)
	{
		bool progress = false;
		enum content_payload_result result = content_payload_cleanup_step(
			&runtime.payload, &progress, &error);

		if (result != CONTENT_PAYLOAD_OK || !clock_ms(NULL, &now))
			break;
	}
	bool clean = !runtime.payload.policy.usage.items;

	if (!clean)
	{
		fputs("fvwm-clipboard-capture: native cleanup unconfirmed\n", stderr);
		/* Metadata must remain alive while a mapping/charge survives. */
		_exit(1);
	}
	if (!x_failed)
	{
		/* Once all callbacks are over, closing our private connection retires
		 * its observer/requestors; no ownership/SAVE_TARGETS mutation. */
		selection_source_close(&runtime.observer.policy);
		XCloseDisplay(runtime.display);
	}
	free(slots);
	free(items);
	return runtime.failed || x_failed ? 1 : 0;
}
