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
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	size_t allocations;
	size_t frees;
} allocation_tracker;

typedef struct
{
	size_t attempts;
	size_t fail_at;
	size_t allocations;
	size_t frees;
} failure_tracker;

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

static void *
failure_malloc(void *context, size_t size)
{
	failure_tracker *tracker = context;
	void		*pointer;

	if (tracker->attempts++ == tracker->fail_at) {
		return NULL;
	}
	pointer = malloc(size);
	if (pointer != NULL) {
		tracker->allocations++;
	}
	return pointer;
}

static void *
failure_calloc(void *context, size_t count, size_t size)
{
	failure_tracker *tracker = context;
	void		*pointer;

	if (tracker->attempts++ == tracker->fail_at) {
		return NULL;
	}
	pointer = calloc(count, size);
	if (pointer != NULL) {
		tracker->allocations++;
	}
	return pointer;
}

static void
failure_free(void *context, void *pointer)
{
	failure_tracker *tracker = context;

	if (pointer != NULL) {
		tracker->frees++;
	}
	free(pointer);
}

static monitor_topology_allocator
make_failure_allocator(failure_tracker *tracker)
{
	monitor_topology_allocator allocator = {
	    .context   = tracker,
	    .malloc_fn = failure_malloc,
	    .calloc_fn = failure_calloc,
	    .free_fn   = failure_free,
	};

	return allocator;
}

static monitor_topology_monitor
make_monitor(Atom name_atom, char *name, int x, int y, int width, int height,
    int mm_width, int mm_height, Bool primary, RROutput *outputs,
    int output_count)
{
	monitor_topology_monitor monitor = {
	    .name_atom	  = name_atom,
	    .name	  = name,
	    .x		  = x,
	    .y		  = y,
	    .width	  = width,
	    .height	  = height,
	    .mm_width	  = mm_width,
	    .mm_height	  = mm_height,
	    .primary	  = primary,
	    .outputs	  = outputs,
	    .output_count = output_count,
	};

	return monitor;
}

static monitor_topology_snapshot *
create_snapshot(allocation_tracker *tracker,
    const monitor_topology_monitor *monitors, int monitor_count)
{
	monitor_topology_allocator allocator = make_allocator(tracker);

	return monitor_topology_snapshot_create(
	    monitors, monitor_count, &allocator);
}

static void
assert_rejected_without_publication(allocation_tracker *tracker,
    monitor_topology_snapshot			      **committed,
    const monitor_topology_monitor *monitors, int monitor_count)
{
	monitor_topology_snapshot *original = *committed;
	uint64_t		   epoch    = original->epoch;
	monitor_topology_snapshot *candidate =
	    create_snapshot(tracker, monitors, monitor_count);

	assert(candidate == NULL);
	assert(monitor_topology_snapshot_publish(committed, candidate)
	       == MONITOR_TOPOLOGY_PUBLISH_INVALID);
	assert(*committed == original);
	assert((*committed)->epoch == epoch);
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

static void
test_malformed_facts_leave_committed_snapshot_untouched(void)
{
	allocation_tracker tracker	 = {0};
	RROutput	   outputs[]	 = {10, 20};
	RROutput	   none_output[] = {None};
	char invalid_utf8[]   = {'b', 'a', 'd', (char)0xc0, (char)0x80, '\0'};
	char truncated_utf8[] = {'b', 'a', 'd', (char)0xe2, (char)0x82, '\0'};
	monitor_topology_monitor valid = make_monitor(
	    10, "monitor-a", 0, 0, 1920, 1080, 510, 290, True, outputs, 2);
	monitor_topology_monitor   invalid;
	monitor_topology_snapshot *initial =
	    create_snapshot(&tracker, &valid, 1);
	monitor_topology_snapshot *committed = NULL;
	monitor_topology_monitor   duplicate_atoms[2];

	assert(initial != NULL);
	assert(monitor_topology_snapshot_publish(&committed, initial)
	       == MONITOR_TOPOLOGY_PUBLISH_INITIAL);
	assert(committed->epoch == 1);

	assert_rejected_without_publication(&tracker, &committed, NULL, 1);
	assert_rejected_without_publication(&tracker, &committed, &valid, 0);
	assert_rejected_without_publication(&tracker, &committed, &valid, -1);

	invalid		  = valid;
	invalid.name_atom = None;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid	     = valid;
	invalid.name = NULL;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid	     = valid;
	invalid.name = "";
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid	     = valid;
	invalid.name = invalid_utf8;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid	     = valid;
	invalid.name = truncated_utf8;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid	      = valid;
	invalid.width = 0;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid	       = valid;
	invalid.height = 0;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid	      = valid;
	invalid.x     = INT_MAX;
	invalid.width = 2;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid	       = valid;
	invalid.y      = INT_MAX;
	invalid.height = 2;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid		 = valid;
	invalid.mm_width = -1;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid		  = valid;
	invalid.mm_height = -1;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid		= valid;
	invalid.primary = 2;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid		     = valid;
	invalid.output_count = -1;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid		= valid;
	invalid.outputs = NULL;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);
	invalid		     = valid;
	invalid.outputs	     = none_output;
	invalid.output_count = 1;
	assert_rejected_without_publication(&tracker, &committed, &invalid, 1);

	duplicate_atoms[0] = valid;
	duplicate_atoms[1] = make_monitor(
	    10, "monitor-b", 1920, 0, 1280, 1024, 0, INT_MAX, False, NULL, 0);
	assert_rejected_without_publication(
	    &tracker, &committed, duplicate_atoms, 2);

	monitor_topology_snapshot_destroy(committed);
	monitor_topology_snapshot_destroy(NULL);
	assert(tracker.allocations == tracker.frees);
}

static void
test_physical_dimensions_are_preserved_without_plausibility_policy(void)
{
	allocation_tracker	 tracker     = {0};
	monitor_topology_monitor monitors[2] = {
	    make_monitor(
		10, "monitor-a", 0, 0, 1920, 1080, 0, 0, True, NULL, 0),
	    make_monitor(20, "monitor-b", 1920, 0, 1280, 1024, INT_MAX, INT_MAX,
		False, NULL, 0),
	};
	monitor_topology_snapshot *snapshot =
	    create_snapshot(&tracker, monitors, 2);

	assert(snapshot != NULL);
	assert(snapshot->monitors[0].mm_width == 0);
	assert(snapshot->monitors[0].mm_height == 0);
	assert(snapshot->monitors[1].mm_width == INT_MAX);
	assert(snapshot->monitors[1].mm_height == INT_MAX);

	monitor_topology_snapshot_destroy(snapshot);
	assert(tracker.allocations == tracker.frees);
}

static void
test_order_and_duplicate_outputs_are_semantically_canonical(void)
{
	allocation_tracker	 tracker	= {0};
	RROutput		 outputs_a[]	= {30, 10, 30, 20};
	RROutput		 outputs_b[]	= {20, 30, 10};
	monitor_topology_monitor first_order[2] = {
	    make_monitor(
		20, "monitor-b", 1920, 0, 1280, 1024, 340, 270, False, NULL, 0),
	    make_monitor(10, "monitor-a", 0, 0, 1920, 1080, 510, 290, True,
		outputs_a, 4),
	};
	monitor_topology_monitor second_order[2] = {
	    make_monitor(10, "monitor-a", 0, 0, 1920, 1080, 510, 290, True,
		outputs_b, 3),
	    make_monitor(
		20, "monitor-b", 1920, 0, 1280, 1024, 340, 270, False, NULL, 0),
	};
	monitor_topology_snapshot *first =
	    create_snapshot(&tracker, first_order, 2);
	monitor_topology_snapshot *second =
	    create_snapshot(&tracker, second_order, 2);

	assert(first != NULL);
	assert(second != NULL);
	assert(first->monitors[0].name_atom == 10);
	assert(first->monitors[0].output_count == 3);
	assert(first->monitors[0].outputs[0] == 10);
	assert(first->monitors[0].outputs[1] == 20);
	assert(first->monitors[0].outputs[2] == 30);
	assert(first->monitors[1].output_count == 0);
	assert(first->monitors[1].outputs == NULL);
	assert(monitor_topology_snapshot_equal(first, second));

	monitor_topology_snapshot_destroy(first);
	monitor_topology_snapshot_destroy(second);
	assert(tracker.allocations == tracker.frees);
}

static void
test_each_semantic_fact_changes_equality(void)
{
	allocation_tracker	 tracker	   = {0};
	RROutput		 outputs[]	   = {10, 20};
	RROutput		 changed_outputs[] = {10, 30};
	monitor_topology_monitor base		   = make_monitor(
		 10, "monitor-a", -10, 20, 1920, 1080, 510, 290, True, outputs, 2);
	monitor_topology_monitor   changed[11];
	monitor_topology_snapshot *original;
	size_t			   index;

	for (index = 0; index < sizeof(changed) / sizeof(changed[0]); index++) {
		changed[index] = base;
	}
	changed[0].name_atom = 11;
	changed[1].name	     = "monitor-renamed";
	changed[2].x++;
	changed[3].y++;
	changed[4].width++;
	changed[5].height++;
	changed[6].mm_width++;
	changed[7].mm_height++;
	changed[8].primary    = False;
	changed[9].outputs    = changed_outputs;
	changed[10].name_atom = 11;
	changed[10].name      = "monitor-renamed";

	original = create_snapshot(&tracker, &base, 1);
	assert(original != NULL);
	for (index = 0; index < sizeof(changed) / sizeof(changed[0]); index++) {
		monitor_topology_snapshot *candidate =
		    create_snapshot(&tracker, &changed[index], 1);

		assert(candidate != NULL);
		assert(!monitor_topology_snapshot_equal(original, candidate));
		monitor_topology_snapshot_destroy(candidate);
	}

	monitor_topology_snapshot_destroy(original);
	assert(tracker.allocations == tracker.frees);
}

static void
test_epoch_overflow_refuses_replacement(void)
{
	allocation_tracker	   tracker   = {0};
	RROutput		   outputs[] = {10};
	monitor_topology_snapshot *committed =
	    make_snapshot(&tracker, 0, outputs, 1);
	monitor_topology_snapshot *candidate;
	monitor_topology_snapshot *original;

	assert(committed != NULL);
	committed->epoch = UINT64_MAX;
	original	 = committed;
	candidate	 = make_snapshot(&tracker, 1, outputs, 1);
	assert(candidate != NULL);
	assert(monitor_topology_snapshot_publish(&committed, candidate)
	       == MONITOR_TOPOLOGY_PUBLISH_INVALID);
	assert(committed == original);
	assert(committed->epoch == UINT64_MAX);

	monitor_topology_snapshot_destroy(committed);
	assert(tracker.allocations == tracker.frees);
}

static void
test_every_allocator_failure_is_recoverable(void)
{
	allocation_tracker	 committed_tracker = {0};
	RROutput		 outputs_a[]	   = {10, 20};
	RROutput		 outputs_b[]	   = {30};
	monitor_topology_monitor monitors[2]	   = {
		  make_monitor(10, "monitor-a", 0, 0, 1920, 1080, 510, 290, True,
		      outputs_a, 2),
		  make_monitor(20, "monitor-b", 1920, 0, 1280, 1024, 340, 270, False,
		      outputs_b, 1),
	      };
	failure_tracker		   successful = {.fail_at = SIZE_MAX};
	monitor_topology_allocator successful_allocator =
	    make_failure_allocator(&successful);
	monitor_topology_snapshot *snapshot = monitor_topology_snapshot_create(
	    monitors, 2, &successful_allocator);
	monitor_topology_snapshot *initial =
	    make_snapshot(&committed_tracker, 0, outputs_a, 2);
	monitor_topology_snapshot *committed = NULL;
	monitor_topology_snapshot *original;
	size_t			   allocation_count;
	size_t			   fail_at;

	assert(snapshot != NULL);
	assert(initial != NULL);
	assert(monitor_topology_snapshot_publish(&committed, initial)
	       == MONITOR_TOPOLOGY_PUBLISH_INITIAL);
	original	 = committed;
	allocation_count = successful.attempts;
	assert(allocation_count > 0);
	monitor_topology_snapshot_destroy(snapshot);
	assert(successful.allocations == successful.frees);

	for (fail_at = 0; fail_at < allocation_count; fail_at++) {
		failure_tracker		   tracker = {.fail_at = fail_at};
		monitor_topology_allocator allocator =
		    make_failure_allocator(&tracker);
		monitor_topology_snapshot *candidate =
		    monitor_topology_snapshot_create(monitors, 2, &allocator);

		assert(candidate == NULL);
		assert(monitor_topology_snapshot_publish(&committed, candidate)
		       == MONITOR_TOPOLOGY_PUBLISH_INVALID);
		assert(committed == original);
		assert(committed->epoch == 1);
		monitor_topology_snapshot_destroy(candidate);
		assert(tracker.allocations == tracker.frees);
	}

	{
		monitor_topology_allocator invalid_allocator = {0};

		assert(monitor_topology_snapshot_create(
			   monitors, 2, &invalid_allocator)
		       == NULL);
	}

	monitor_topology_snapshot_destroy(committed);
	assert(committed_tracker.allocations == committed_tracker.frees);
}

int
main(void)
{
	test_first_publication_has_epoch_one();
	test_equal_publication_preserves_pointer_and_epoch();
	test_changed_publication_advances_epoch_once();
	test_malformed_facts_leave_committed_snapshot_untouched();
	test_physical_dimensions_are_preserved_without_plausibility_policy();
	test_order_and_duplicate_outputs_are_semantically_canonical();
	test_each_semantic_fact_changes_equality();
	test_epoch_overflow_refuses_replacement();
	test_every_allocator_failure_is_recoverable();

	return 0;
}
