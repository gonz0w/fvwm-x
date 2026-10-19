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

#define FSCREEN_TESTING 1
#include "libs/monitor_types.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	XRRMonitorInfo *reply;
	int		monitor_count;
	int		query_count;
	int		reply_free_count;
	int		name_query_count;
	int		name_free_count;
	size_t		allocation_count;
	size_t		fail_allocation;
	size_t		live_allocations;
	int root_width;
	int root_height;
	int root_queries;
} randr_fixture;

typedef struct
{
	size_t allocation_count;
	size_t fail_allocation;
	size_t live_monitors;
	size_t live_screens;
	size_t live_names;
} legacy_fixture;

static void *
fixture_malloc(void *context, size_t size)
{
	randr_fixture *fixture = context;
	void	      *pointer;

	fixture->allocation_count++;
	if (fixture->allocation_count == fixture->fail_allocation) {
		return NULL;
	}
	pointer = malloc(size);
	if (pointer != NULL) {
		fixture->live_allocations++;
	}
	return pointer;
}

static void *
fixture_calloc(void *context, size_t count, size_t size)
{
	randr_fixture *fixture = context;
	void	      *pointer;

	fixture->allocation_count++;
	if (fixture->allocation_count == fixture->fail_allocation) {
		return NULL;
	}
	pointer = calloc(count, size);
	if (pointer != NULL) {
		fixture->live_allocations++;
	}
	return pointer;
}

static void
fixture_free(void *context, void *pointer)
{
	randr_fixture *fixture = context;

	if (pointer != NULL) {
		assert(fixture->live_allocations > 0);
		fixture->live_allocations--;
	}
	free(pointer);
}

static XRRMonitorInfo *
fixture_get_monitors(
    void *context, Display *display, Window root, Bool active, int *count)
{
	randr_fixture *fixture = context;

	(void)display;
	assert(root == (Window)99);
	assert(active == True);
	fixture->query_count++;
	*count = fixture->monitor_count;
	return fixture->reply;
}

static char *
fixture_get_atom_name(void *context, Display *display, Atom atom)
{
	randr_fixture *fixture = context;
	char	      *name;

	(void)display;
	fixture->name_query_count++;
	if (atom == (Atom)999) {
		return NULL;
	}
	name = malloc(16);
	assert(name != NULL);
	(void)snprintf(name, 16, "monitor-%lu", atom);
	return name;
}

static void
fixture_free_atom_name(void *context, char *name)
{
	randr_fixture *fixture = context;

	fixture->name_free_count++;
	free(name);
}

static void
fixture_free_monitors(void *context, XRRMonitorInfo *monitors)
{
	randr_fixture *fixture = context;

	assert(monitors == fixture->reply);
	fixture->reply_free_count++;
}

static bool
fixture_get_root_monitor(void *context, Display *display, Window root,
    monitor_topology_monitor *monitor)
{
	randr_fixture *fixture = context;

	assert(root == (Window)99);
	fixture->root_queries++;
	*monitor = (monitor_topology_monitor){
	    .name_atom = (Atom)600,
	    .name = "FVWM-X-Root",
	    .width = fixture->root_width,
	    .height = fixture->root_height,
	    .primary = True,
	};
	return fixture->root_width > 0 && fixture->root_height > 0;
}

static fscreen_randr_adapter
make_adapter(randr_fixture *fixture)
{
	return (fscreen_randr_adapter){
	    .context	    = fixture,
	    .get_monitors   = fixture_get_monitors,
	    .get_atom_name  = fixture_get_atom_name,
	    .free_atom_name = fixture_free_atom_name,
	    .free_monitors  = fixture_free_monitors,
	    .get_root_monitor = fixture_get_root_monitor,
	    .topology_allocator =
		{
		    .context   = fixture,
		    .malloc_fn = fixture_malloc,
		    .calloc_fn = fixture_calloc,
		    .free_fn   = fixture_free,
		},
	};
}

static bool
legacy_should_fail(legacy_fixture *fixture)
{
	fixture->allocation_count++;
	return fixture->allocation_count == fixture->fail_allocation;
}

static struct monitor *
legacy_allocate_monitor(void *context)
{
	legacy_fixture *fixture = context;
	struct monitor *monitor;

	if (legacy_should_fail(fixture)) {
		return NULL;
	}
	monitor = calloc(1, sizeof(*monitor));
	if (monitor != NULL) {
		fixture->live_monitors++;
	}
	return monitor;
}

static struct screen_info *
legacy_allocate_screen(void *context)
{
	legacy_fixture	   *fixture = context;
	struct screen_info *screen;

	if (legacy_should_fail(fixture)) {
		return NULL;
	}
	screen = calloc(1, sizeof(*screen));
	if (screen != NULL) {
		fixture->live_screens++;
	}
	return screen;
}

static char *
legacy_copy_name(void *context, const char *source)
{
	legacy_fixture *fixture = context;
	size_t		size;
	char	       *name;

	if (legacy_should_fail(fixture)) {
		return NULL;
	}
	size = strlen(source) + 1;
	name = malloc(size);
	if (name != NULL) {
		memcpy(name, source, size);
		fixture->live_names++;
	}
	return name;
}

static void
legacy_free_monitor(void *context, struct monitor *monitor)
{
	legacy_fixture *fixture = context;

	if (monitor != NULL) {
		assert(fixture->live_monitors > 0);
		fixture->live_monitors--;
	}
	free(monitor);
}

static void
legacy_free_screen(void *context, struct screen_info *screen)
{
	legacy_fixture *fixture = context;

	if (screen != NULL) {
		assert(fixture->live_screens > 0);
		fixture->live_screens--;
	}
	free(screen);
}

static void
legacy_free_name(void *context, char *name)
{
	legacy_fixture *fixture = context;

	if (name != NULL) {
		assert(fixture->live_names > 0);
		fixture->live_names--;
	}
	free(name);
}

static fscreen_legacy_allocator
make_legacy_allocator(legacy_fixture *fixture)
{
	return (fscreen_legacy_allocator){
	    .context	      = fixture,
	    .allocate_monitor = legacy_allocate_monitor,
	    .allocate_screen  = legacy_allocate_screen,
	    .copy_name	      = legacy_copy_name,
	    .free_monitor     = legacy_free_monitor,
	    .free_screen      = legacy_free_screen,
	    .free_name	      = legacy_free_name,
	};
}

static XRRMonitorInfo
make_monitor(Atom atom, RROutput *outputs, int output_count)
{
	return (XRRMonitorInfo){
	    .name      = atom,
	    .primary   = True,
	    .automatic = False,
	    .noutput   = output_count,
	    .x	       = -1600,
	    .y	       = 40,
	    .width     = 1600,
	    .height    = 900,
	    .mwidth    = 510,
	    .mheight   = 290,
	    .outputs   = outputs,
	};
}

static void
destroy_committed(monitor_topology_snapshot **committed, randr_fixture *fixture)
{
	monitor_topology_snapshot_destroy(*committed);
	*committed = NULL;
	assert(fixture->live_allocations == 0);
}

static uint64_t
digest_value(uint64_t digest, uintptr_t value)
{
	digest ^= (uint64_t)value;
	return digest * UINT64_C(1099511628211);
}

static void
assert_safe_effective_scale(const struct monitor *monitor)
{
	assert(monitor->effective_scale.factor.quarters
	       == MONITOR_SCALE_QUARTERS_MIN);
	assert(monitor->effective_scale.source == MONITOR_SCALE_SOURCE_SAFE);
	assert(
	    monitor->effective_scale.match_status == MONITOR_SCALE_MATCH_NONE);
	assert(monitor->effective_scale.auto_status
	       == MONITOR_SCALE_AUTO_MISSING_AXIS);
	assert(
	    monitor->effective_scale.boundary == MONITOR_SCALE_BOUNDARY_NONE);
	assert(monitor->effective_scale.reason
	       == MONITOR_SCALE_REASON_SAFE_DEFAULT);
}

static uint64_t
legacy_digest(
    struct monitors *legacy_monitors, struct screen_infos *legacy_screens)
{
	uint64_t	    digest = UINT64_C(1469598103934665603);
	struct monitor	   *monitor;
	struct screen_info *screen;

	RB_FOREACH(monitor, monitors, legacy_monitors)
	{
		const unsigned char *name =
		    (const unsigned char *)monitor->si->name;

		digest = digest_value(digest, (uintptr_t)monitor);
		digest = digest_value(digest, (uintptr_t)monitor->si);
		digest = digest_value(digest, (uintptr_t)monitor->si->name);
		digest = digest_value(digest, monitor->si->name_atom);
		digest = digest_value(digest, (unsigned int)monitor->flags);
		digest = digest_value(digest, (unsigned int)monitor->emit);
		digest = digest_value(digest, (unsigned int)monitor->number);
		digest = digest_value(digest, (unsigned int)monitor->si->x);
		digest = digest_value(digest, (unsigned int)monitor->si->y);
		digest = digest_value(digest, (unsigned int)monitor->si->w);
		digest = digest_value(digest, (unsigned int)monitor->si->h);
		digest = digest_value(digest, monitor->si->rr_output);
		digest = digest_value(
		    digest, monitor->effective_scale.factor.quarters);
		digest = digest_value(digest, monitor->effective_scale.source);
		digest =
		    digest_value(digest, monitor->effective_scale.auto_status);
		digest =
		    digest_value(digest, monitor->effective_scale.boundary);
		digest = digest_value(digest, monitor->effective_scale.reason);
		while (*name != '\0') {
			digest = digest_value(digest, *name++);
		}
	}
	TAILQ_FOREACH(screen, legacy_screens, entry)
	{
		digest = digest_value(digest, (uintptr_t)screen);
	}
	return digest;
}

static void
destroy_legacy(struct monitors	   *legacy_monitors,
    struct screen_infos		   *legacy_screens,
    const fscreen_legacy_allocator *allocator)
{
	struct monitor *monitor;
	struct monitor *next;

	RB_FOREACH_SAFE(monitor, monitors, legacy_monitors, next)
	{
		RB_REMOVE(monitors, legacy_monitors, monitor);
		TAILQ_REMOVE(legacy_screens, monitor->si, entry);
		allocator->free_name(
		    allocator->context, (char *)monitor->si->name);
		allocator->free_screen(allocator->context, monitor->si);
		allocator->free_monitor(allocator->context, monitor);
	}
}

static void
test_failed_empty_unresolved_and_invalid_queries_retain_state(void)
{
	RROutput	      outputs[] = {31};
	XRRMonitorInfo	      monitor	= make_monitor((Atom)12, outputs, 1);
	randr_fixture	      fixture = {.reply = &monitor, .monitor_count = 1};
	fscreen_randr_adapter adapter = make_adapter(&fixture);
	monitor_topology_snapshot	*committed = NULL;
	const monitor_topology_snapshot *before;

	assert(fscreen_refresh_topology_with_adapters(
		   NULL, (Window)99, &committed, NULL, NULL, &adapter, NULL)
	       == FSCREEN_TOPOLOGY_REFRESH_CHANGED);
	before = committed;
	assert(before->epoch == 1);

	fixture.reply	      = NULL;
	fixture.monitor_count = -1;
	assert(fscreen_refresh_topology_with_adapters(
		   NULL, (Window)99, &committed, NULL, NULL, &adapter, NULL)
	       == FSCREEN_TOPOLOGY_REFRESH_FAILED);
	assert(committed == before && committed->epoch == 1);

	fixture.reply	      = &monitor;
	fixture.monitor_count = 0;
	assert(fscreen_refresh_topology_with_adapters(
		   NULL, (Window)99, &committed, NULL, NULL, &adapter, NULL)
	       == FSCREEN_TOPOLOGY_REFRESH_FAILED);
	assert(committed == before && committed->epoch == 1);

	fixture.monitor_count = 1;
	monitor.name	      = (Atom)999;
	assert(fscreen_refresh_topology_with_adapters(
		   NULL, (Window)99, &committed, NULL, NULL, &adapter, NULL)
	       == FSCREEN_TOPOLOGY_REFRESH_FAILED);
	assert(committed == before && committed->epoch == 1);

	monitor.name  = (Atom)12;
	monitor.width = 0;
	assert(fscreen_refresh_topology_with_adapters(
		   NULL, (Window)99, &committed, NULL, NULL, &adapter, NULL)
	       == FSCREEN_TOPOLOGY_REFRESH_FAILED);
	assert(committed == before && committed->epoch == 1);

	monitor.width		 = 1600;
	fixture.allocation_count = 0;
	fixture.fail_allocation	 = 1;
	assert(fscreen_refresh_topology_with_adapters(
		   NULL, (Window)99, &committed, NULL, NULL, &adapter, NULL)
	       == FSCREEN_TOPOLOGY_REFRESH_FAILED);
	assert(committed == before && committed->epoch == 1);
	fixture.fail_allocation = 0;

	assert(fixture.query_count == 6);
	destroy_committed(&committed, &fixture);
}

static void
test_equal_and_changed_candidates_have_exact_generation_results(void)
{
	RROutput	      outputs[] = {44, 22};
	XRRMonitorInfo	      monitor	= make_monitor((Atom)8, outputs, 2);
	randr_fixture	      fixture = {.reply = &monitor, .monitor_count = 1};
	fscreen_randr_adapter adapter = make_adapter(&fixture);
	monitor_topology_snapshot	*committed = NULL;
	const monitor_topology_snapshot *first;

	assert(fscreen_refresh_topology_with_adapters(
		   NULL, (Window)99, &committed, NULL, NULL, &adapter, NULL)
	       == FSCREEN_TOPOLOGY_REFRESH_CHANGED);
	first = committed;
	assert(first->epoch == 1);
	assert(first->monitors[0].output_count == 2);
	assert(first->monitors[0].outputs[0] == (RROutput)22);
	assert(first->monitors[0].outputs[1] == (RROutput)44);

	assert(fscreen_refresh_topology_with_adapters(
		   NULL, (Window)99, &committed, NULL, NULL, &adapter, NULL)
	       == FSCREEN_TOPOLOGY_REFRESH_UNCHANGED);
	assert(committed == first && committed->epoch == 1);

	monitor.x++;
	assert(fscreen_refresh_topology_with_adapters(
		   NULL, (Window)99, &committed, NULL, NULL, &adapter, NULL)
	       == FSCREEN_TOPOLOGY_REFRESH_CHANGED);
	assert(committed != first && committed->epoch == 2);
	assert(fixture.query_count == 3);
	assert(fixture.reply_free_count == 3);
	assert(fixture.name_query_count == 3);
	assert(fixture.name_free_count == 3);
	destroy_committed(&committed, &fixture);
}

static void
test_extreme_legacy_coordinates_do_not_overflow(void)
{
	struct screen_info minimum_screen = {
	    .name = "minimum",
	    .x	  = INT_MIN,
	    .y	  = INT_MIN,
	    .w	  = 1,
	    .h	  = 1,
	};
	struct screen_info zero_screen = {
	    .name = "zero",
	    .x	  = 0,
	    .y	  = 0,
	    .w	  = INT_MAX,
	    .h	  = INT_MAX,
	};
	struct monitor minimum = {.si = &minimum_screen};
	struct monitor zero    = {.si = &zero_screen};

	assert(monitor_compare(&minimum, &zero) < 0);
	assert(monitor_compare(&zero, &minimum) > 0);
	minimum_screen.y = zero_screen.y;
	assert(monitor_compare(&minimum, &zero) < 0);
	assert(monitor_compare(&zero, &minimum) > 0);
	minimum_screen.y = INT_MIN;
	assert(fscreen_monitor_distance_to_point(&minimum, INT_MAX, INT_MAX)
	       == INT64_C(8589934588));
}

static void
test_every_legacy_staging_allocation_failure_retains_state(void)
{
	RROutput       first_outputs[]	= {32, 31};
	RROutput       second_outputs[] = {41};
	XRRMonitorInfo replies[]	= {
		   make_monitor((Atom)12, first_outputs, 2),
		   make_monitor((Atom)13, second_outputs, 1),
	       };
	randr_fixture		 randr = {.reply = replies, .monitor_count = 1};
	legacy_fixture		 legacy_alloc = {0};
	fscreen_randr_adapter	 adapter      = make_adapter(&randr);
	fscreen_legacy_allocator allocator =
	    make_legacy_allocator(&legacy_alloc);
	monitor_topology_snapshot	*committed = NULL;
	struct monitors			 legacy_monitors;
	struct screen_infos		 legacy_screens;
	struct monitor			*first_monitor;
	struct monitor			*iter;
	const monitor_topology_snapshot *before;
	uint64_t			 before_digest;
	size_t				 fail_index;

	RB_INIT(&legacy_monitors);
	TAILQ_INIT(&legacy_screens);
	assert(
	    fscreen_refresh_topology_with_adapters(NULL, (Window)99, &committed,
		&legacy_monitors, &legacy_screens, &adapter, &allocator)
	    == FSCREEN_TOPOLOGY_REFRESH_CHANGED);
	assert(committed->epoch == 1);
	assert(legacy_alloc.allocation_count == 3);
	first_monitor = RB_MIN(monitors, &legacy_monitors);
	assert(first_monitor != NULL);
	assert(first_monitor->si->name_atom == (Atom)12);
	assert_safe_effective_scale(first_monitor);
	before	      = committed;
	before_digest = legacy_digest(&legacy_monitors, &legacy_screens);

	randr.monitor_count = 2;
	for (fail_index = 1; fail_index <= 3; fail_index++) {
		legacy_alloc.allocation_count = 0;
		legacy_alloc.fail_allocation  = fail_index;
		assert(fscreen_refresh_topology_with_adapters(NULL, (Window)99,
			   &committed, &legacy_monitors, &legacy_screens,
			   &adapter, &allocator)
		       == FSCREEN_TOPOLOGY_REFRESH_FAILED);
		assert(committed == before && committed->epoch == 1);
		assert(legacy_digest(&legacy_monitors, &legacy_screens)
		       == before_digest);
		assert_safe_effective_scale(first_monitor);
		assert(legacy_alloc.live_monitors == 1);
		assert(legacy_alloc.live_screens == 1);
		assert(legacy_alloc.live_names == 1);
	}

	legacy_alloc.allocation_count = 0;
	legacy_alloc.fail_allocation  = 0;
	assert(
	    fscreen_refresh_topology_with_adapters(NULL, (Window)99, &committed,
		&legacy_monitors, &legacy_screens, &adapter, &allocator)
	    == FSCREEN_TOPOLOGY_REFRESH_CHANGED);
	assert(committed->epoch == 2);
	assert(RB_MIN(monitors, &legacy_monitors) == first_monitor);
	RB_FOREACH(iter, monitors, &legacy_monitors)
	{
		assert_safe_effective_scale(iter);
	}
	assert(
	    fscreen_monitor_by_output(committed, &legacy_monitors, (RROutput)32)
	    == first_monitor);

	randr.reply		      = &replies[1];
	randr.monitor_count	      = 1;
	legacy_alloc.allocation_count = 0;
	assert(
	    fscreen_refresh_topology_with_adapters(NULL, (Window)99, &committed,
		&legacy_monitors, &legacy_screens, &adapter, &allocator)
	    == FSCREEN_TOPOLOGY_REFRESH_CHANGED);
	assert(first_monitor->flags & MONITOR_DISABLED);
	assert(first_monitor->emit == MONITOR_DISABLED);
	assert(legacy_alloc.allocation_count == 0);

	replies[1].x++;
	assert(
	    fscreen_refresh_topology_with_adapters(NULL, (Window)99, &committed,
		&legacy_monitors, &legacy_screens, &adapter, &allocator)
	    == FSCREEN_TOPOLOGY_REFRESH_CHANGED);
	assert(first_monitor->flags & MONITOR_DISABLED);
	assert(first_monitor->emit == 0);

	destroy_legacy(&legacy_monitors, &legacy_screens, &allocator);
	destroy_committed(&committed, &randr);
	assert(legacy_alloc.live_monitors == 0);
	assert(legacy_alloc.live_screens == 0);
	assert(legacy_alloc.live_names == 0);
}

static void
test_root_fallback_resizes_and_yields_to_real_monitors(void)
{
	randr_fixture fixture = {.monitor_count = -1,
	    .root_width = 800, .root_height = 600};
	fscreen_randr_adapter adapter = make_adapter(&fixture);
	monitor_topology_snapshot *committed = NULL;
	const monitor_topology_snapshot *before;
	RROutput outputs[] = {31};
	XRRMonitorInfo monitor = make_monitor((Atom)12, outputs, 1);

	assert(fscreen_refresh_topology_with_adapters(NULL, (Window)99,
		&committed, NULL, NULL, &adapter, NULL) ==
		FSCREEN_TOPOLOGY_REFRESH_FAILED);
	assert(committed == NULL && fixture.root_queries == 0);
	fixture.monitor_count = 0;
	assert(fscreen_refresh_topology_with_adapters(NULL, (Window)99,
		&committed, NULL, NULL, &adapter, NULL) ==
		FSCREEN_TOPOLOGY_REFRESH_CHANGED);
	assert(committed->root_fallback && committed->epoch == 1);
	assert(committed->monitor_count == 1);
	assert(committed->monitors[0].width == 800);
	assert(committed->monitors[0].height == 600);
	assert(committed->monitors[0].output_count == 0);
	assert(committed->monitors[0].primary);
	before = committed;
	assert(fscreen_refresh_topology_with_adapters(NULL, (Window)99,
		&committed, NULL, NULL, &adapter, NULL) ==
		FSCREEN_TOPOLOGY_REFRESH_UNCHANGED);
	assert(committed == before && committed->epoch == 1);
	fixture.root_width = 1024;
	assert(fscreen_refresh_topology_with_adapters(NULL, (Window)99,
		&committed, NULL, NULL, &adapter, NULL) ==
		FSCREEN_TOPOLOGY_REFRESH_CHANGED);
	assert(committed->root_fallback && committed->epoch == 2);
	assert(committed->monitors[0].width == 1024);
	before = committed;
	fixture.root_width = 0;
	assert(fscreen_refresh_topology_with_adapters(NULL, (Window)99,
		&committed, NULL, NULL, &adapter, NULL) ==
		FSCREEN_TOPOLOGY_REFRESH_FAILED);
	assert(committed == before && committed->epoch == 2);
	fixture.root_width = 1024;
	fixture.allocation_count = 0;
	fixture.fail_allocation = 1;
	assert(fscreen_refresh_topology_with_adapters(NULL, (Window)99,
		&committed, NULL, NULL, &adapter, NULL) ==
		FSCREEN_TOPOLOGY_REFRESH_FAILED);
	assert(committed == before && committed->epoch == 2);
	fixture.fail_allocation = 0;
	fixture.reply = &monitor;
	fixture.monitor_count = 1;
	assert(fscreen_refresh_topology_with_adapters(NULL, (Window)99,
		&committed, NULL, NULL, &adapter, NULL) ==
		FSCREEN_TOPOLOGY_REFRESH_CHANGED);
	assert(!committed->root_fallback && committed->epoch == 3);
	assert(committed->monitors[0].name_atom == (Atom)12);
	before = committed;
	fixture.reply = NULL;
	fixture.monitor_count = 0;
	int root_queries = fixture.root_queries;
	assert(fscreen_refresh_topology_with_adapters(NULL, (Window)99,
		&committed, NULL, NULL, &adapter, NULL) ==
		FSCREEN_TOPOLOGY_REFRESH_FAILED);
	assert(committed == before && fixture.root_queries == root_queries);
	destroy_committed(&committed, &fixture);
}

int
main(void)
{
	test_root_fallback_resizes_and_yields_to_real_monitors();
	test_failed_empty_unresolved_and_invalid_queries_retain_state();
	test_equal_and_changed_candidates_have_exact_generation_results();
	test_extreme_legacy_coordinates_do_not_overflow();
	test_every_legacy_staging_allocation_failure_retains_state();
	return 0;
}
