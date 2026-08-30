/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

#include "config.h"

#include "libs/monitor_topology.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	size_t allocations;
	size_t frees;
} allocation_tracker;

static void *
tracked_malloc(void *context, size_t size)
{
	allocation_tracker *tracker = context;
	void		   *pointer = malloc(size);

	if (pointer != NULL) {
		tracker->allocations++;
	}
	return pointer;
}

static void *
tracked_calloc(void *context, size_t count, size_t size)
{
	allocation_tracker *tracker = context;
	void		   *pointer = calloc(count, size);

	if (pointer != NULL) {
		tracker->allocations++;
	}
	return pointer;
}

static void
tracked_free(void *context, void *pointer)
{
	allocation_tracker *tracker = context;

	if (pointer != NULL) {
		tracker->frees++;
	}
	free(pointer);
}

static monitor_topology_allocator
make_allocator(allocation_tracker *tracker)
{
	monitor_topology_allocator allocator = {
	    .context   = tracker,
	    .malloc_fn = tracked_malloc,
	    .calloc_fn = tracked_calloc,
	    .free_fn   = tracked_free,
	};

	return allocator;
}

static monitor_topology_snapshot *
make_snapshot(allocation_tracker *tracker, int x, const RROutput *outputs,
    int output_count)
{
	monitor_topology_allocator allocator = make_allocator(tracker);
	monitor_topology_monitor   source    = {
		 .name_atom    = 42,
		 .name	       = NULL,
		 .x	       = x,
		 .y	       = -20,
		 .width	       = 1920,
		 .height       = 1080,
		 .mm_width     = 510,
		 .mm_height    = 290,
		 .primary      = True,
		 .outputs      = NULL,
		 .output_count = output_count,
	     };
	monitor_topology_snapshot *snapshot;

	source.name = malloc(sizeof("monitor-a"));
	assert(source.name != NULL);
	memcpy(source.name, "monitor-a", sizeof("monitor-a"));
	if (output_count > 0) {
		source.outputs =
		    malloc((size_t)output_count * sizeof(*outputs));
		assert(source.outputs != NULL);
		memcpy(source.outputs, outputs,
		    (size_t)output_count * sizeof(*outputs));
	}

	snapshot = monitor_topology_snapshot_create(&source, 1, &allocator);
	memset(source.name, 'x', strlen(source.name));
	free(source.name);
	free(source.outputs);
	return snapshot;
}

static void
test_first_publication_has_epoch_one(void)
{
	allocation_tracker	   tracker   = {0};
	const RROutput		   outputs[] = {30, 10, 30};
	monitor_topology_snapshot *candidate =
	    make_snapshot(&tracker, 100, outputs, 3);
	monitor_topology_snapshot *committed = NULL;

	assert(candidate != NULL);
	assert(monitor_topology_snapshot_validate(candidate));
	assert(strcmp(candidate->monitors[0].name, "monitor-a") == 0);
	assert(candidate->monitors[0].output_count == 2);
	assert(candidate->monitors[0].outputs[0] == 10);
	assert(candidate->monitors[0].outputs[1] == 30);
	assert(monitor_topology_snapshot_publish(&committed, candidate)
	       == MONITOR_TOPOLOGY_PUBLISH_INITIAL);
	assert(committed == candidate);
	assert(committed->epoch == 1);

	monitor_topology_snapshot_destroy(committed);
	assert(tracker.allocations == tracker.frees);
}

static void
test_equal_publication_preserves_pointer_and_epoch(void)
{
	allocation_tracker	   tracker	   = {0};
	const RROutput		   first_outputs[] = {10, 30};
	const RROutput		   equal_outputs[] = {30, 10, 30};
	monitor_topology_snapshot *initial =
	    make_snapshot(&tracker, 100, first_outputs, 2);
	monitor_topology_snapshot *committed = NULL;
	monitor_topology_snapshot *candidate;
	monitor_topology_snapshot *original;
	size_t			   frees_before;

	assert(monitor_topology_snapshot_publish(&committed, initial)
	       == MONITOR_TOPOLOGY_PUBLISH_INITIAL);
	original  = committed;
	candidate = make_snapshot(&tracker, 100, equal_outputs, 3);
	assert(candidate != NULL);
	assert(monitor_topology_snapshot_equal(committed, candidate));
	frees_before = tracker.frees;
	assert(monitor_topology_snapshot_publish(&committed, candidate)
	       == MONITOR_TOPOLOGY_PUBLISH_UNCHANGED);
	assert(committed == original);
	assert(committed->epoch == 1);
	assert(tracker.frees > frees_before);

	monitor_topology_snapshot_destroy(committed);
	assert(tracker.allocations == tracker.frees);
}

static void
test_changed_publication_advances_epoch_once(void)
{
	allocation_tracker	   tracker   = {0};
	const RROutput		   outputs[] = {10, 30};
	monitor_topology_snapshot *initial =
	    make_snapshot(&tracker, 100, outputs, 2);
	monitor_topology_snapshot *committed = NULL;
	monitor_topology_snapshot *old;
	monitor_topology_snapshot *candidate;
	size_t			   frees_before;

	assert(monitor_topology_snapshot_publish(&committed, initial)
	       == MONITOR_TOPOLOGY_PUBLISH_INITIAL);
	old	  = committed;
	candidate = make_snapshot(&tracker, 101, outputs, 2);
	assert(candidate != NULL);
	assert(!monitor_topology_snapshot_equal(committed, candidate));
	frees_before = tracker.frees;
	assert(monitor_topology_snapshot_publish(&committed, candidate)
	       == MONITOR_TOPOLOGY_PUBLISH_CHANGED);
	assert(committed == candidate);
	assert(committed != old);
	assert(committed->epoch == 2);
	assert(tracker.frees > frees_before);

	monitor_topology_snapshot_destroy(committed);
	assert(tracker.allocations == tracker.frees);
}

int
main(void)
{
	test_first_publication_has_epoch_one();
	test_equal_publication_preserves_pointer_and_epoch();
	test_changed_publication_advances_epoch_once();

	return 0;
}
