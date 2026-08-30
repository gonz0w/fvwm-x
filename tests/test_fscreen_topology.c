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
#include "libs/FScreen.h"

#include <assert.h>
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
} randr_fixture;

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

static fscreen_randr_adapter
make_adapter(randr_fixture *fixture)
{
	return (fscreen_randr_adapter){
	    .context	    = fixture,
	    .get_monitors   = fixture_get_monitors,
	    .get_atom_name  = fixture_get_atom_name,
	    .free_atom_name = fixture_free_atom_name,
	    .free_monitors  = fixture_free_monitors,
	    .topology_allocator =
		{
		    .context   = fixture,
		    .malloc_fn = fixture_malloc,
		    .calloc_fn = fixture_calloc,
		    .free_fn   = fixture_free,
		},
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

int
main(void)
{
	test_failed_empty_unresolved_and_invalid_queries_retain_state();
	test_equal_and_changed_candidates_have_exact_generation_results();
	return 0;
}
