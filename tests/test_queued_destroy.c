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

/* This queue unit does not reproduce server XID reuse. The same-XID lifetime
 * race is assessed from production property-reply ordering and ownership. */
static XEvent queue[256], appended_event;
static unsigned int queued, reads, snapshots, restores;
static Bool append_during_read;

int __wrap_XEventsQueued(Display *display, int mode)
{
	(void)display;
	assert(mode == QueuedAlready);
	snapshots++;
	return (int)queued;
}

int __wrap_XNextEvent(Display *display, XEvent *event)
{
	(void)display;
	assert(queued > 0);
	memcpy(event, queue, sizeof(*event));
	queued--;
	memmove(queue, queue + 1, queued * sizeof(*queue));
	reads++;
	if (append_during_read)
	{
		assert(queued < 256);
		memcpy(queue + queued++, &appended_event, sizeof(appended_event));
		append_during_read = False;
	}
	return 0;
}

int __wrap_XPutBackEvent(Display *display, XEvent *event)
{
	(void)display;
	assert(queued < 256);
	memmove(queue + 1, queue, queued * sizeof(*queue));
	queued++;
	memcpy(queue, event, sizeof(*event));
	restores++;
	return 0;
}

static XEvent event_record(int type, Window window, unsigned long serial)
{
	XEvent event;

	/* Check untouched union bytes as well as the fields used for matching. */
	memset(&event, 0x5a, sizeof(event));
	event.xany.type = type;
	event.xany.display = NULL;
	event.xany.serial = serial;
	event.xany.send_event = False;
	event.xany.window = window;
	if (type == DestroyNotify)
	{
		event.xdestroywindow.event = 7;
		event.xdestroywindow.window = window;
	}
	else if (type == PropertyNotify)
		event.xproperty.time = 999;
	return event;
}

static void reset(void)
{
	queued = reads = snapshots = restores = 0;
	append_during_read = False;
}

static void fill(unsigned int count)
{
	for (unsigned int index = 0; index < count; index++)
		queue[queued++] = event_record(
			index % 2 ? ClientMessage : PropertyNotify, 42, 100 + index);
}

static void probe(FQueuedDestroyResult expected)
{
	XEvent original[256];
	unsigned int count = queued;
	unsigned int prefix = count < FEV_DESTROY_BATCH_MAX ?
		count : FEV_DESTROY_BATCH_MAX;
	unsigned int prior_reads = reads, prior_restores = restores;
	unsigned int prior_snapshots = snapshots;

	memcpy(original, queue, count * sizeof(*queue));
	if (append_during_read)
	{
		assert(count > 0 && count < 256);
		memcpy(original + count++, &appended_event, sizeof(appended_event));
	}
	assert(FCheckQueuedWindowDestroy(NULL, 42, 100) == expected);
	assert(snapshots == prior_snapshots + 1);
	assert(reads == prior_reads + prefix);
	assert(restores == prior_restores + prefix);
	assert(queued == count);
	assert(memcmp(original, queue, count * sizeof(*queue)) == 0);
}

static void test_empty_and_clear_snapshot(void)
{
	reset();
	probe(FEV_QUEUED_DESTROY_CLEAR);
	fill(FEV_DESTROY_BATCH_MAX);
	fev_init_invalid_event_type(LASTEvent + 100);
	queue[0].type = fev_invalid_event_type;
	probe(FEV_QUEUED_DESTROY_CLEAR);
}

static void test_actual_and_refused_destroy_records(void)
{
	XEvent event;

	for (unsigned int variant = 0; variant < 6; variant++)
	{
		reset();
		fill(5);
		event = event_record(DestroyNotify, 42, 100);
		if (variant == 1)
			event.xdestroywindow.serial = 101;
		else if (variant == 2)
			event.xdestroywindow.send_event = True;
		else if (variant == 3)
			event.xdestroywindow.serial = 99;
		else if (variant == 4)
		{
			/* The recipient is not the destroyed raw window. */
			event.xdestroywindow.event = 42;
			event.xdestroywindow.window = 43;
		}
		else if (variant == 5)
			event.xdestroywindow.type = UnmapNotify;
		memcpy(queue + 2, &event, sizeof(event));
		probe(variant < 2 ? FEV_QUEUED_DESTROY_MATCH :
			FEV_QUEUED_DESTROY_CLEAR);
	}
}

static void test_finite_prefix_and_later_dispatch(void)
{
	XEvent event;

	reset();
	fill(2 * FEV_DESTROY_BATCH_MAX);
	queue[FEV_DESTROY_BATCH_MAX] = event_record(DestroyNotify, 42, 100);
	probe(FEV_QUEUED_DESTROY_INCOMPLETE);
	/* Rechecking cannot accidentally look beyond the unchanged prefix. */
	probe(FEV_QUEUED_DESTROY_INCOMPLETE);
	for (unsigned int index = 0; index < FEV_DESTROY_BATCH_MAX; index++)
		FNextEvent(NULL, &event);
	probe(FEV_QUEUED_DESTROY_MATCH);

	reset();
	fill(FEV_DESTROY_BATCH_MAX + 1);
	queue[FEV_DESTROY_BATCH_MAX - 1] =
		event_record(DestroyNotify, 42, 100);
	/* A matching event in the inspected prefix wins over incompleteness. */
	probe(FEV_QUEUED_DESTROY_MATCH);
}

static void test_appended_tail_is_outside_snapshot(void)
{
	unsigned int counts[] = {3, FEV_DESTROY_BATCH_MAX + 1};

	for (unsigned int index = 0; index < 2; index++)
	{
		reset();
		fill(counts[index]);
		appended_event = event_record(DestroyNotify, 42, 100);
		append_during_read = True;
		probe(index == 0 ? FEV_QUEUED_DESTROY_CLEAR :
			FEV_QUEUED_DESTROY_INCOMPLETE);
		if (index == 0)
			probe(FEV_QUEUED_DESTROY_MATCH);
	}
}

static void test_cached_current_previous_and_time(void)
{
	XEvent current, before, after;
	XEvent old = event_record(ClientMessage, 7, 10);
	Time timestamp = fev_get_evtime() + 123;
	void *saved;

	reset();
	fev_fake_event(&old);
	queue[queued++] = event_record(PropertyNotify, 42, 11);
	queue[0].xproperty.time = timestamp;
	FNextEvent(NULL, &current);
	fev_get_last_event(&before);
	assert(fev_get_evtime() == timestamp);
	fill(3);
	queue[0].xproperty.time = timestamp + 1000;
	queue[1] = event_record(DestroyNotify, 42, 100);
	saved = fev_save_event();
	probe(FEV_QUEUED_DESTROY_MATCH);
	fev_get_last_event(&after);
	assert(memcmp(&before, &after, sizeof(before)) == 0);
	assert(fev_get_evtime() == timestamp);
	fev_fake_event(queue);
	fev_restore_event(saved);
	fev_get_last_event(&after);
	assert(memcmp(&before, &after, sizeof(before)) == 0);
	/* The previous cached event must survive the probe and save/restore. */
	FPutBackEvent(NULL, &current);
	fev_get_last_event(&after);
	assert(memcmp(&old, &after, sizeof(old)) == 0);
	assert(fev_get_evtime() == timestamp);
}

int main(void)
{
	assert(FEV_DESTROY_BATCH_MAX == 64);
	test_empty_and_clear_snapshot();
	test_actual_and_refused_destroy_records();
	test_finite_prefix_and_later_dispatch();
	test_appended_tail_is_outside_snapshot();
	test_cached_current_previous_and_time();
	puts("queued-destroy: bounded snapshot, matching, FIFO, cache/time passed");
	return 0;
}
