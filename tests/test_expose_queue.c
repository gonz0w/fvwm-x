/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "libs/FEvent.h"

static XEvent queue[2048];
static int queued, reads, snapshots, restores;
static Bool append_during_read;

int __wrap_XEventsQueued(Display *display, int mode)
{
	assert(mode == QueuedAlready);
	snapshots++;
	return queued;
}

int __wrap_XNextEvent(Display *display, XEvent *event)
{
	assert(queued > 0);
	*event = queue[0];
	memmove(queue, queue + 1, --queued * sizeof(*queue));
	reads++;
	if (append_during_read)
	{
		queue[queued++] = (XEvent){.xexpose = {
			.type = Expose, .window = 1, .serial = 999}};
		append_during_read = False;
	}
	return 0;
}

int __wrap_XPutBackEvent(Display *display, XEvent *event)
{
	assert(queued < 2048);
	memmove(queue + 1, queue, queued++ * sizeof(*queue));
	queue[0] = *event;
	restores++;
	return 0;
}

static void reset(void)
{
	queued = reads = snapshots = restores = 0;
	append_during_read = False;
}

static void test_bounded_backlog(void)
{
	XEvent selected[FEV_EXPOSE_BATCH_MAX];

	reset();
	for (int index = 0; index < 1000; index++)
	{
		queue[queued++] = (XEvent){.xexpose = {
			.type = Expose, .window = 1, .serial = index}};
	}
	assert(FCollectQueuedExposes(NULL, None, selected) == 32);
	assert(reads == 32 && snapshots == 1 && restores == 0);
	assert(queued == 968 && queue[0].xany.serial == 32);
	for (int index = 0; index < 32; index++)
	{
		assert(selected[index].xany.serial == (unsigned long)index);
	}
}

static void test_window_selection_and_fifo(void)
{
	XEvent selected[FEV_EXPOSE_BATCH_MAX];
	unsigned long expected[] = {0, 1, 3, 4, 6};

	reset();
	for (int index = 0; index < 7; index++)
	{
		queue[queued++] = (XEvent){.xexpose = {
			.type = index % 3 == 0 ? PropertyNotify : Expose,
			.window = index % 3 == 1 ? 2 : 1, .serial = index}};
	}
	assert(FCollectQueuedExposes(NULL, 1, selected) == 2);
	assert(selected[0].xany.serial == 2);
	assert(selected[1].xany.serial == 5);
	assert(queued == 5 && reads == 7 && restores == 5);
	for (int index = 0; index < queued; index++)
	{
		assert(queue[index].xany.serial == expected[index]);
	}
}

static void test_snapshot_and_invalid_records(void)
{
	XEvent selected[FEV_EXPOSE_BATCH_MAX];

	reset();
	assert(FCollectQueuedExposes(NULL, None, selected) == 0);
	assert(reads == 0 && snapshots == 1);
	fev_init_invalid_event_type(LASTEvent + 100);
	queue[queued++] = (XEvent){.type = fev_invalid_event_type};
	queue[queued++] = (XEvent){.xexpose = {
		.type = Expose, .window = 1, .serial = 1}};
	append_during_read = True;
	assert(FCollectQueuedExposes(NULL, None, selected) == 1);
	assert(reads == 2 && queued == 2);
	assert(FEV_IS_EVENT_INVALID(queue[0]));
	assert(queue[1].xany.serial == 999);
	assert(FCollectQueuedExposes(NULL, None, selected) == 1);
	assert(selected[0].xany.serial == 999 && queued == 1);
}

static void test_foreign_prefix_is_finite(void)
{
	XEvent selected[FEV_EXPOSE_BATCH_MAX];

	reset();
	for (int index = 0; index < 64; index++)
	{
		queue[queued++] = (XEvent){.xany = {
			.type = index >= 32 ? Expose :
				(index % 2 ? ConfigureNotify : DestroyNotify),
			.window = 1, .serial = index}};
	}
	assert(FCollectQueuedExposes(NULL, None, selected) == 0);
	assert(reads == 32 && restores == 32 && queued == 64);
	for (int index = 0; index < 64; index++)
	{
		assert(queue[index].xany.serial == (unsigned long)index);
	}
}

static void test_cached_events_and_time(void)
{
	XEvent current, before, after, selected[FEV_EXPOSE_BATCH_MAX];
	XEvent old = {.type = ClientMessage};
	void *saved;

	reset();
	fev_fake_event(&old);
	queue[queued++] = (XEvent){.xproperty = {
		.type = PropertyNotify, .time = 123}};
	FNextEvent(NULL, &current);
	fev_get_last_event(&before);
	assert(fev_get_evtime() == 123);
	queue[queued++] = (XEvent){.xproperty = {
		.type = PropertyNotify, .time = 999}};
	queue[queued++] = (XEvent){.type = Expose};
	saved = fev_save_event();
	assert(FCollectQueuedExposes(NULL, None, selected) == 1);
	fev_get_last_event(&after);
	assert(memcmp(&before, &after, sizeof(before)) == 0);
	assert(fev_get_evtime() == 123);
	fev_fake_event(selected);
	fev_restore_event(saved);
	fev_get_last_event(&after);
	assert(memcmp(&before, &after, sizeof(before)) == 0);
	/* Put-back observes the restored previous event too. */
	FPutBackEvent(NULL, &current);
	fev_get_last_event(&after);
	assert(memcmp(&old, &after, sizeof(old)) == 0);
	assert(fev_get_evtime() == 123);
}

int main(void)
{
	test_bounded_backlog();
	test_window_selection_and_fifo();
	test_snapshot_and_invalid_records();
	test_foreign_prefix_is_finite();
	test_cached_events_and_time();
	puts("expose-queue: bounded snapshot, FIFO, cache/time passed");
	return 0;
}
