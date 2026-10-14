/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <string.h>
#define FEVENT_PRIVILEGED_ACCESS
#include "libs/FEvent.h"

static XEvent queue[256];
static unsigned queued, reads, peeks, snapshots;
static Bool append_during_read;

int __wrap_XEventsQueued(Display *display, int mode)
{
	assert(mode == QueuedAlready);
	snapshots++;
	return queued;
}

int __wrap_XPeekEvent(Display *display, XEvent *event)
{
	assert(queued > 0);
	*event = queue[0];
	peeks++;
	return 0;
}

int __wrap_XNextEvent(Display *display, XEvent *event)
{
	assert(queued > 0);
	*event = queue[0];
	memmove(queue, queue + 1, --queued * sizeof(*queue));
	reads++;
	if (append_during_read)
	{
		queue[queued++] = *event;
		append_during_read = False;
	}
	return 0;
}

static XEvent resize_event(Window window, unsigned long mask, int value)
{
	XEvent event = {.xconfigurerequest = {.type = ConfigureRequest,
		.window = window, .value_mask = mask, .width = value,
		.height = value + 1, .serial = value}};

	return event;
}

static void reset(void)
{
	queued = reads = peeks = snapshots = 0;
	append_during_read = False;
}

static void test_latest_partial_dimensions_and_cached_dispatch(void)
{
	XEvent current = resize_event(42, CWWidth | CWHeight, 100);
	XEvent cached, after;

	reset();
	queue[queued++] = resize_event(42, CWWidth, 200);
	queue[queued++] = resize_event(42, CWHeight, 300);
	fev_fake_event(&current);
	fev_copy_last_event(&cached);
	Time timestamp = fev_get_evtime();
	assert(FMergeQueuedResizeRequests(NULL, &current, 32) == 2);
	assert(current.xconfigurerequest.width == 200);
	assert(current.xconfigurerequest.height == 301);
	assert(current.xconfigurerequest.value_mask == (CWWidth | CWHeight));
	assert(current.xconfigurerequest.serial == 300);
	assert(queued == 0 && reads == 2 && peeks == 2 && snapshots == 1);
	fev_copy_last_event(&after);
	assert(memcmp(&cached, &after, sizeof(cached)) == 0);
	assert(fev_get_evtime() == timestamp);
}

static void test_barriers_preserve_order_and_contents(void)
{
	XEvent barriers[] = {
		resize_event(99, CWWidth, 200),
		resize_event(42, CWWidth | CWX, 200),
		resize_event(42, CWHeight | CWY, 200),
		resize_event(42, CWWidth | CWBorderWidth, 200),
		resize_event(42, CWWidth | CWStackMode, 200),
		resize_event(42, CWWidth | CWSibling, 200),
		{.xproperty = {.type = PropertyNotify, .window = 42}},
		{.xdestroywindow = {.type = DestroyNotify, .window = 42}},
		{.xreparent = {.type = ReparentNotify, .window = 42}},
		{.xunmap = {.type = UnmapNotify, .window = 42}},
		{.xbutton = {.type = ButtonPress, .window = 42}},
	};

	for (size_t i = 0; i < sizeof(barriers) / sizeof(barriers[0]); i++)
	{
		XEvent current = resize_event(42, CWWidth | CWHeight, 100);
		XEvent saved[2];

		reset();
		queue[queued++] = resize_event(42, CWWidth, 150);
		queue[queued++] = barriers[i];
		queue[queued++] = resize_event(42, CWHeight, 300);
		memcpy(saved, queue + 1, sizeof(saved));
		assert(FMergeQueuedResizeRequests(NULL, &current, 32) == 1);
		assert(current.xconfigurerequest.width == 150);
		assert(current.xconfigurerequest.height == 101);
		assert(queued == 2 && reads == 1 && peeks == 2);
		assert(memcmp(saved, queue, sizeof(saved)) == 0);
	}
}

static void test_budget_and_snapshot_leave_continuation(void)
{
	XEvent current = resize_event(42, CWWidth | CWHeight, 100);

	reset();
	for (int i = 0; i < 128; i++)
	{
		queue[queued++] = resize_event(42, CWWidth | CWHeight, 200 + i);
	}
	assert(FMergeQueuedResizeRequests(NULL, &current, 32) == 32);
	assert(queued == 96 && reads == 32 && peeks == 32 && snapshots == 1);
	assert(current.xconfigurerequest.width == 231);
	assert(queue[0].xconfigurerequest.width == 232);
	reset();
	queue[queued++] = resize_event(42, CWWidth, 400);
	append_during_read = True;
	assert(FMergeQueuedResizeRequests(NULL, &current, 32) == 1);
	assert(queued == 1 && reads == 1 && snapshots == 1);
}

static void test_empty_and_ineligible_current_request(void)
{
	XEvent current = resize_event(42, CWWidth | CWHeight, 100);

	reset();
	assert(FMergeQueuedResizeRequests(NULL, &current, 32) == 0);
	assert(snapshots == 1 && reads == 0 && peeks == 0);
	queue[queued++] = resize_event(42, CWWidth, 200);
	assert(FMergeQueuedResizeRequests(NULL, &current, 0) == 0);
	assert(snapshots == 1);
	current.xconfigurerequest.value_mask |= CWX;
	assert(FMergeQueuedResizeRequests(NULL, &current, 32) == 0);
	assert(snapshots == 1 && queued == 1);
}

int main(void)
{
	test_latest_partial_dimensions_and_cached_dispatch();
	test_barriers_preserve_order_and_contents();
	test_budget_and_snapshot_leave_continuation();
	test_empty_and_ineligible_current_request();
	return 0;
}
