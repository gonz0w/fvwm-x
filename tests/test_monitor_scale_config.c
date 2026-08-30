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

#include "fvwm/execcontext.h"
#include "fvwm/fvwm.h"

#include "fvwm/commands.h"
#include "fvwm/monitor_scale_config.h"
#include "libs/FScreen.h"
#include "libs/Parse.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	size_t allocation_count;
	size_t fail_allocation;
	size_t live_allocations;
	size_t diagnostic_count;
	char   reason[128];
	char   name[128];
	char   lines[16][512];
} monitor_scale_config_test_fixture;

static void *
test_scale_alloc(void *context, size_t size)
{
	monitor_scale_config_test_fixture *fixture = context;
	void				  *pointer;

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

static void
test_scale_free(void *context, void *pointer)
{
	monitor_scale_config_test_fixture *fixture = context;

	if (pointer != NULL) {
		assert(fixture->live_allocations > 0);
		fixture->live_allocations--;
	}
	free(pointer);
}

static void
test_scale_diagnostic(void *context, const char *reason, const char *name)
{
	monitor_scale_config_test_fixture *fixture = context;
	size_t line_index			   = fixture->diagnostic_count;

	fixture->diagnostic_count++;
	(void)snprintf(fixture->reason, sizeof(fixture->reason), "%s",
	    reason == NULL ? "" : reason);
	(void)snprintf(fixture->name, sizeof(fixture->name), "%s",
	    name == NULL ? "" : name);
	if (line_index < 16) {
		(void)snprintf(fixture->lines[line_index],
		    sizeof(fixture->lines[line_index]), "%s%s%s",
		    reason == NULL ? "" : reason, name == NULL ? "" : " ",
		    name == NULL ? "" : name);
	}
}

static monitor_scale_config *
initialize_fixture(monitor_scale_config_test_fixture *fixture)
{
	monitor_scale_config_allocator allocator = {
	    .allocate	= test_scale_alloc,
	    .deallocate = test_scale_free,
	    .context	= fixture,
	};

	monitor_scale_config_initialize_with(
	    &allocator, test_scale_diagnostic, fixture);
	return monitor_scale_config_get();
}

static uint64_t
digest_byte(uint64_t digest, unsigned char byte)
{
	digest ^= byte;
	return digest * UINT64_C(1099511628211);
}

static uint64_t
config_digest(const monitor_scale_config *config)
{
	const monitor_scale_override *override;
	uint64_t		      digest = UINT64_C(1469598103934665603);

	for (override = config->overrides; override != NULL;
	    override  = override->next) {
		const unsigned char *name =
		    (const unsigned char *) override->name;

		while (*name != '\0') {
			digest = digest_byte(digest, *name++);
		}
		digest = digest_byte(digest, 0);
		digest = digest_byte(digest, override->factor.quarters);
	}
	digest = digest_byte(digest, config->has_fallback);
	digest = digest_byte(digest, config->fallback.quarters);
	digest = digest_byte(digest, config->finalized);
	return digest;
}

static monitor_scale_override *
find_override(monitor_scale_config *config, const char *name)
{
	monitor_scale_override *override;

	for (override = config->overrides; override != NULL;
	    override  = override->next) {
		if (strcmp(override->name, name) == 0) {
			return override;
		}
	}
	return NULL;
}

static void
run_scale_command(void (*command)(F_CMD_ARGS), char *action);

static bool
scale_results_equal(
    const monitor_scale_result *left, const monitor_scale_result *right)
{
	return left->factor.quarters == right->factor.quarters
	       && left->source == right->source
	       && left->match_status == right->match_status
	       && left->auto_status == right->auto_status
	       && left->boundary == right->boundary
	       && left->reason == right->reason;
}

static monitor_topology_monitor
make_monitor_fact(const char *name, Atom name_atom, int width, int height,
    int mm_width, int mm_height)
{
	monitor_topology_monitor fact = {
	    .name      = (char *)name,
	    .name_atom = name_atom,
	    .width     = width,
	    .height    = height,
	    .mm_width  = mm_width,
	    .mm_height = mm_height,
	};

	return fact;
}

static struct monitor
make_runtime_monitor(
    struct screen_info *screen, const char *name, Atom name_atom, int x)
{
	struct monitor monitor = {0};

	screen->name	  = name;
	screen->name_atom = name_atom;
	screen->x	  = x;
	screen->w	  = 100;
	screen->h	  = 100;
	monitor.si	  = screen;
	monitor.effective_scale =
	    (monitor_scale_result)MONITOR_SCALE_RESULT_SAFE_INIT;
	return monitor;
}

static void
test_association_resolves_every_source_and_retains_rejection_metadata(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	monitor_topology_monitor	  facts[] = {
	     make_monitor_fact("override", 1, 1920, 1080, 254, 143),
	     make_monitor_fact("automatic", 2, 1920, 1080, 254, 143),
	     make_monitor_fact("fallback", 3, 1920, 1080, 0, 0),
	     make_monitor_fact("safe", 4, 1920, 1080, 0, 0),
	 };
	monitor_topology_snapshot snapshot = {
	    .monitors	   = facts,
	    .monitor_count = 4,
	};
	struct screen_info screens[4] = {0};
	struct monitor	   monitors[4];
	int		   i;

	assert(
	    monitor_scale_config_declare_override(config, "override", "1.25"));
	assert(monitor_scale_config_declare_fallback(config, "1.75"));
	for (i = 0; i < 4; i++) {
		monitors[i] = make_runtime_monitor(
		    &screens[i], facts[i].name, facts[i].name_atom, i * 100);
		monitor_scale_config_associate_monitor(
		    config, &facts[i], &snapshot, &monitors[i]);
	}
	assert(monitors[0].effective_scale.source
	       == MONITOR_SCALE_SOURCE_OVERRIDE);
	assert(monitors[0].effective_scale.factor.quarters == 5);
	assert(monitors[0].effective_scale.match_status
	       == MONITOR_SCALE_MATCH_UNIQUE);
	assert(monitors[1].effective_scale.source
	       == MONITOR_SCALE_SOURCE_AUTOMATIC);
	assert(monitors[1].effective_scale.factor.quarters == 8);
	assert(monitors[2].effective_scale.source
	       == MONITOR_SCALE_SOURCE_FALLBACK);
	assert(monitors[2].effective_scale.auto_status
	       == MONITOR_SCALE_AUTO_MISSING_AXIS);
	assert(monitors[2].effective_scale.reason
	       == MONITOR_SCALE_REASON_CONFIGURED_FALLBACK);

	config->has_fallback = false;
	monitor_scale_config_associate_monitor(
	    config, &facts[3], &snapshot, &monitors[3]);
	assert(monitors[3].effective_scale.source == MONITOR_SCALE_SOURCE_SAFE);
	assert(monitors[3].effective_scale.factor.quarters == 4);
	assert(monitors[3].effective_scale.auto_status
	       == MONITOR_SCALE_AUTO_MISSING_AXIS);

	monitor_scale_config_shutdown();
	assert(fixture.live_allocations == 0);
}

static void
test_association_rejects_ambiguous_and_retains_unmatched_overrides(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	monitor_topology_monitor	  facts[] = {
	     make_monitor_fact("duplicate", 1, 1920, 1080, 0, 0),
	     make_monitor_fact("duplicate", 2, 1920, 1080, 0, 0),
	     make_monitor_fact("active", 3, 1920, 1080, 0, 0),
	 };
	monitor_topology_snapshot snapshot = {
	    .monitors	   = facts,
	    .monitor_count = 3,
	};
	struct screen_info duplicate_screen = {0};
	struct screen_info active_screen    = {0};
	struct monitor	   duplicate =
	    make_runtime_monitor(&duplicate_screen, "duplicate", 1, 0);
	struct monitor active =
	    make_runtime_monitor(&active_screen, "active", 3, 100);

	assert(monitor_scale_config_declare_override(config, "duplicate", "2"));
	assert(monitor_scale_config_declare_override(config, "docked", "3"));
	assert(monitor_scale_config_declare_fallback(config, "1.5"));
	monitor_scale_config_associate_monitor(
	    config, &facts[0], &snapshot, &duplicate);
	assert(
	    duplicate.effective_scale.source == MONITOR_SCALE_SOURCE_FALLBACK);
	assert(duplicate.effective_scale.match_status
	       == MONITOR_SCALE_MATCH_AMBIGUOUS);
	monitor_scale_config_associate_monitor(
	    config, &facts[2], &snapshot, &active);
	assert(active.effective_scale.source == MONITOR_SCALE_SOURCE_FALLBACK);
	assert(active.effective_scale.match_status == MONITOR_SCALE_MATCH_NONE);
	assert(find_override(config, "docked") != NULL);

	monitor_scale_config_shutdown();
	assert(fixture.live_allocations == 0);
}

static void
test_association_preserves_automatic_endpoint_reasons(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	monitor_topology_monitor	  facts[] = {
	     make_monitor_fact("low", 1, 500, 500, 254, 254),
	     make_monitor_fact("high", 2, 5000, 5000, 254, 254),
	 };
	monitor_topology_snapshot snapshot = {
	    .monitors	   = facts,
	    .monitor_count = 2,
	};
	struct screen_info screens[2] = {0};
	struct monitor	   low = make_runtime_monitor(&screens[0], "low", 1, 0);
	struct monitor high = make_runtime_monitor(&screens[1], "high", 2, 100);

	monitor_scale_config_associate_monitor(
	    config, &facts[0], &snapshot, &low);
	monitor_scale_config_associate_monitor(
	    config, &facts[1], &snapshot, &high);
	assert(low.effective_scale.source == MONITOR_SCALE_SOURCE_AUTOMATIC);
	assert(low.effective_scale.boundary == MONITOR_SCALE_BOUNDARY_LOW);
	assert(low.effective_scale.reason
	       == MONITOR_SCALE_REASON_AUTOMATIC_BOUND_LOW);
	assert(high.effective_scale.source == MONITOR_SCALE_SOURCE_AUTOMATIC);
	assert(high.effective_scale.boundary == MONITOR_SCALE_BOUNDARY_HIGH);
	assert(high.effective_scale.reason
	       == MONITOR_SCALE_REASON_AUTOMATIC_BOUND_HIGH);

	monitor_scale_config_shutdown();
}

static void
test_recompute_is_allocation_free_total_and_idempotent(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	monitor_topology_monitor	  facts[] = {
	     make_monitor_fact("left", 1, 1920, 1080, 254, 143),
	     make_monitor_fact("right", 2, 1920, 1080, 0, 0),
	 };
	monitor_topology_snapshot snapshot = {
	    .monitors	   = facts,
	    .monitor_count = 2,
	    .epoch	   = 19,
	};
	struct monitors	   runtime_monitors;
	struct screen_info screens[2] = {0};
	struct monitor	   runtime[]  = {
		 make_runtime_monitor(&screens[0], "left", 1, 0),
		 make_runtime_monitor(&screens[1], "right", 2, 100),
	     };
	monitor_scale_result	  first[2];
	monitor_topology_monitor *snapshot_pointer = snapshot.monitors;
	size_t			  allocations;
	uint64_t		  epoch = snapshot.epoch;

	RB_INIT(&runtime_monitors);
	assert(RB_INSERT(monitors, &runtime_monitors, &runtime[0]) == NULL);
	assert(RB_INSERT(monitors, &runtime_monitors, &runtime[1]) == NULL);
	assert(monitor_scale_config_declare_fallback(config, "1.5"));
	allocations = fixture.allocation_count;
	assert(monitor_scale_config_recompute_with(
	    config, &snapshot, &runtime_monitors));
	assert(fixture.allocation_count == allocations);
	assert(monitor_scale_config_results_valid_with(
	    &snapshot, &runtime_monitors));
	first[0] = runtime[0].effective_scale;
	first[1] = runtime[1].effective_scale;
	assert(monitor_scale_config_recompute_with(
	    config, &snapshot, &runtime_monitors));
	assert(scale_results_equal(&first[0], &runtime[0].effective_scale));
	assert(scale_results_equal(&first[1], &runtime[1].effective_scale));
	assert(snapshot.monitors == snapshot_pointer);
	assert(snapshot.epoch == epoch);

	monitor_scale_config_shutdown();
	assert(fixture.live_allocations == 0);
}

static void
test_finalize_owns_one_store_and_rejects_late_mutation(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	monitor_topology_monitor	  fact =
	    make_monitor_fact("active", 1, 1920, 1080, 0, 0);
	monitor_topology_snapshot snapshot = {
	    .monitors	   = &fact,
	    .monitor_count = 1,
	};
	struct monitors	   runtime_monitors;
	struct screen_info screen = {0};
	struct monitor runtime = make_runtime_monitor(&screen, "active", 1, 0);
	monitor_scale_result result_before;
	uint64_t	     config_before;
	char		     late_override[] = "active 2";
	char		     late_fallback[] = "2";
	size_t		     diagnostics_before;

	RB_INIT(&runtime_monitors);
	assert(RB_INSERT(monitors, &runtime_monitors, &runtime) == NULL);
	assert(monitor_scale_config_get() == config);
	assert(monitor_scale_config_declare_override(config, "active", "1.25"));
	assert(monitor_scale_config_declare_override(config, "docked", "3"));
	assert(monitor_scale_config_declare_fallback(config, "1.5"));
	diagnostics_before = fixture.diagnostic_count;
	assert(monitor_scale_config_finalize_initial_with(
	    config, &snapshot, &runtime_monitors));
	assert(monitor_scale_config_get() == config);
	assert(monitor_scale_config_is_finalized(config));
	assert(monitor_scale_config_results_valid_with(
	    &snapshot, &runtime_monitors));
	assert(runtime.effective_scale.source == MONITOR_SCALE_SOURCE_OVERRIDE);
	assert(runtime.effective_scale.factor.quarters == 5);
	assert(fixture.diagnostic_count == diagnostics_before + 2);
	assert(strstr(fixture.reason, "retained-unmatched") != NULL);
	assert(strcmp(fixture.name, "") == 0);

	config_before	   = config_digest(config);
	result_before	   = runtime.effective_scale;
	diagnostics_before = fixture.diagnostic_count;
	run_scale_command(CMD_MonitorScale, late_override);
	run_scale_command(CMD_MonitorScaleFallback, late_fallback);
	assert(monitor_scale_config_get() == config);
	assert(config_digest(config) == config_before);
	assert(scale_results_equal(&result_before, &runtime.effective_scale));
	assert(fixture.diagnostic_count == diagnostics_before + 2);
	assert(strstr(fixture.reason, "finalized") != NULL);

	monitor_scale_config_shutdown();
	assert(monitor_scale_config_get() == config);
	assert(config->overrides == NULL);
	assert(!config->has_fallback);
	assert(!config->finalized);
	assert(config->fallback.quarters == 0);
	assert(fixture.live_allocations == 0);
	assert(initialize_fixture(&fixture) == config);
	assert(config->fallback.quarters == MONITOR_SCALE_QUARTERS_MIN);
	assert(!config->has_fallback && !config->finalized);
	monitor_scale_config_shutdown();
}

static void
test_print_info_emits_authoritative_reason_complete_escaped_output(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	monitor_topology_monitor	  facts[] = {
	     make_monitor_fact("override", 1, 1920, 1080, 254, 143),
	     make_monitor_fact("automatic", 2, 1920, 1080, 254, 143),
	     make_monitor_fact("low", 3, 500, 500, 254, 254),
	     make_monitor_fact("high", 4, 5000, 5000, 254, 254),
	     make_monitor_fact("fallback", 5, 1920, 1080, 0, 0),
	     make_monitor_fact("safe", 6, 1920, 1080, 0, 0),
	     make_monitor_fact("ambiguous", 7, 1920, 1080, 0, 0),
	     make_monitor_fact(
		 "quote\"line\n\t\001-\303\251", 8, 1920, 1080, 10, 143),
	 };
	monitor_topology_snapshot snapshot = {
	    .monitors	   = facts,
	    .monitor_count = 8,
	};
	struct monitors	     runtime_monitors;
	struct screen_info   screens[8] = {0};
	struct monitor	     runtime[8];
	monitor_scale_result results[] = {
	    {{5}, MONITOR_SCALE_SOURCE_OVERRIDE, MONITOR_SCALE_MATCH_UNIQUE,
		MONITOR_SCALE_AUTO_VALID, MONITOR_SCALE_BOUNDARY_NONE,
		MONITOR_SCALE_REASON_EXACT_OVERRIDE},
	    {{8}, MONITOR_SCALE_SOURCE_AUTOMATIC, MONITOR_SCALE_MATCH_NONE,
		MONITOR_SCALE_AUTO_VALID, MONITOR_SCALE_BOUNDARY_NONE,
		MONITOR_SCALE_REASON_VALID_AUTOMATIC},
	    {{4}, MONITOR_SCALE_SOURCE_AUTOMATIC, MONITOR_SCALE_MATCH_NONE,
		MONITOR_SCALE_AUTO_VALID, MONITOR_SCALE_BOUNDARY_LOW,
		MONITOR_SCALE_REASON_AUTOMATIC_BOUND_LOW},
	    {{16}, MONITOR_SCALE_SOURCE_AUTOMATIC, MONITOR_SCALE_MATCH_NONE,
		MONITOR_SCALE_AUTO_VALID, MONITOR_SCALE_BOUNDARY_HIGH,
		MONITOR_SCALE_REASON_AUTOMATIC_BOUND_HIGH},
	    {{7}, MONITOR_SCALE_SOURCE_FALLBACK, MONITOR_SCALE_MATCH_NONE,
		MONITOR_SCALE_AUTO_MISSING_AXIS, MONITOR_SCALE_BOUNDARY_NONE,
		MONITOR_SCALE_REASON_CONFIGURED_FALLBACK},
	    {{4}, MONITOR_SCALE_SOURCE_SAFE, MONITOR_SCALE_MATCH_NONE,
		MONITOR_SCALE_AUTO_MISSING_AXIS, MONITOR_SCALE_BOUNDARY_NONE,
		MONITOR_SCALE_REASON_SAFE_DEFAULT},
	    {{6}, MONITOR_SCALE_SOURCE_FALLBACK, MONITOR_SCALE_MATCH_AMBIGUOUS,
		MONITOR_SCALE_AUTO_MISSING_AXIS, MONITOR_SCALE_BOUNDARY_NONE,
		MONITOR_SCALE_REASON_CONFIGURED_FALLBACK},
	    {{6}, MONITOR_SCALE_SOURCE_FALLBACK, MONITOR_SCALE_MATCH_NONE,
		MONITOR_SCALE_AUTO_X_IMPLAUSIBLE, MONITOR_SCALE_BOUNDARY_NONE,
		MONITOR_SCALE_REASON_CONFIGURED_FALLBACK},
	};
	const char *expected[] = {
	    "Scale monitor name=\"override\" factor=1.25 source=override "
	    "pixels=1920x1080 physical_mm=254x143 match_status=unique "
	    "auto_status=valid boundary=none reason=exact-override",
	    "Scale monitor name=\"automatic\" factor=2 source=automatic "
	    "pixels=1920x1080 physical_mm=254x143 match_status=none "
	    "auto_status=valid boundary=none reason=valid-automatic",
	    "Scale monitor name=\"low\" factor=1 source=automatic "
	    "pixels=500x500 physical_mm=254x254 match_status=none "
	    "auto_status=valid boundary=low reason=automatic-bound-low",
	    "Scale monitor name=\"high\" factor=4 source=automatic "
	    "pixels=5000x5000 physical_mm=254x254 match_status=none "
	    "auto_status=valid boundary=high reason=automatic-bound-high",
	    "Scale monitor name=\"fallback\" factor=1.75 source=fallback "
	    "pixels=1920x1080 physical_mm=0x0 match_status=none "
	    "auto_status=missing-axis boundary=none reason=configured-fallback",
	    "Scale monitor name=\"safe\" factor=1 source=safe "
	    "pixels=1920x1080 physical_mm=0x0 match_status=none "
	    "auto_status=missing-axis boundary=none reason=safe-default",
	    "Scale monitor name=\"ambiguous\" factor=1.5 source=fallback "
	    "pixels=1920x1080 physical_mm=0x0 match_status=ambiguous "
	    "auto_status=missing-axis boundary=none reason=configured-fallback",
	    "Scale monitor name=\"quote\\\"line\\n\\t\\u0001-\303\251\" "
	    "factor=1.5 source=fallback pixels=1920x1080 physical_mm=10x143 "
	    "match_status=none auto_status=x-implausible boundary=none "
	    "reason=configured-fallback",
	    "Scale override name=\"a-docked\" factor=1.25 match_status=none "
	    "state=retained-unmatched",
	    "Scale override name=\"z-docked\" factor=3 match_status=none "
	    "state=retained-unmatched",
	};
	int i;

	RB_INIT(&runtime_monitors);
	for (i = 0; i < 8; i++) {
		runtime[i] = make_runtime_monitor(
		    &screens[i], facts[i].name, facts[i].name_atom, i * 100);
		runtime[i].effective_scale = results[i];
		assert(RB_INSERT(monitors, &runtime_monitors, &runtime[i])
		       == NULL);
	}
	assert(monitor_scale_config_declare_override(config, "z-docked", "3"));
	assert(
	    monitor_scale_config_declare_override(config, "a-docked", "1.25"));
	fixture.diagnostic_count = 0;

	assert(monitor_scale_config_print_info_with(
	    config, &snapshot, &runtime_monitors));
	assert(fixture.diagnostic_count == 10);
	for (i = 0; i < 10; i++) {
		assert(strcmp(fixture.lines[i], expected[i]) == 0);
		assert(strchr(fixture.lines[i], '\n') == NULL);
		assert(strchr(fixture.lines[i], '\r') == NULL);
	}

	monitor_scale_config_shutdown();
	assert(fixture.live_allocations == 0);
}

static void
test_valid_sequential_declarations_and_exact_matching(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	monitor_topology_monitor	  facts[] = {
	     {.name = "DP-1"},
	     {.name = "dp-1"},
	 };
	monitor_topology_snapshot snapshot = {
	    .monitors	   = facts,
	    .monitor_count = 2,
	};
	monitor_scale_factor factor = {0};

	assert(monitor_scale_config_declare_override(config, "DP-1", "1.25"));
	assert(find_override(config, "DP-1")->factor.quarters == 5);
	assert(monitor_scale_config_declare_override(config, "DP-1", "1.75"));
	assert(find_override(config, "DP-1")->factor.quarters == 7);
	assert(monitor_scale_config_declare_override(config, "dp-1", "2"));
	assert(find_override(config, "DP-1") != find_override(config, "dp-1"));
	assert(monitor_scale_config_match_override(
		   config, "DP-1", &snapshot, &factor)
	       == MONITOR_SCALE_MATCH_UNIQUE);
	assert(factor.quarters == 7);
	assert(monitor_scale_config_match_override(
		   config, "dp-1", &snapshot, &factor)
	       == MONITOR_SCALE_MATCH_UNIQUE);
	assert(factor.quarters == 8);

	monitor_scale_config_shutdown();
	assert(fixture.live_allocations == 0);
}

static void
test_invalid_and_allocation_failed_replacements_retain_store(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	uint64_t			  before;
	size_t				  fail_index;

	assert(monitor_scale_config_declare_override(config, "DP-1", "1.5"));
	assert(monitor_scale_config_declare_fallback(config, "1.25"));
	before = config_digest(config);
	assert(!monitor_scale_config_declare_override(config, "DP-1", "1.3"));
	assert(config_digest(config) == before);
	assert(fixture.diagnostic_count == 1);

	for (fail_index = 1; fail_index <= 2; fail_index++) {
		before			 = config_digest(config);
		fixture.allocation_count = 0;
		fixture.fail_allocation	 = fail_index;
		assert(!monitor_scale_config_declare_override(
		    config, "DP-1", "2"));
		assert(config_digest(config) == before);
	}
	fixture.fail_allocation = 0;
	assert(find_override(config, "DP-1")->factor.quarters == 6);
	assert(config->has_fallback && config->fallback.quarters == 5);

	monitor_scale_config_shutdown();
	assert(fixture.live_allocations == 0);
}

static void
test_fallback_replacement_is_independent_and_transactional(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	uint64_t			  before;

	assert(monitor_scale_config_declare_override(config, "DP-1", "1.25"));
	assert(monitor_scale_config_declare_fallback(config, "1.5"));
	assert(monitor_scale_config_declare_fallback(config, "2.25"));
	assert(config->fallback.quarters == 9);
	assert(find_override(config, "DP-1")->factor.quarters == 5);
	before = config_digest(config);
	assert(!monitor_scale_config_declare_fallback(config, "2.50"));
	assert(config_digest(config) == before);

	monitor_scale_config_shutdown();
	assert(fixture.live_allocations == 0);
}

static void
test_zero_unique_and_ambiguous_active_match_outcomes(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	monitor_topology_monitor	  facts[] = {
	     {.name = "OTHER"},
	     {.name = "DP-1"},
	 };
	monitor_topology_snapshot snapshot = {
	    .monitors	   = facts,
	    .monitor_count = 1,
	};
	monitor_scale_factor factor = {99};
	uint64_t	     before;

	assert(monitor_scale_config_declare_override(config, "DP-1", "1.75"));
	before = config_digest(config);
	assert(monitor_scale_config_match_override(
		   config, "DP-1", &snapshot, &factor)
	       == MONITOR_SCALE_MATCH_NONE);
	assert(config_digest(config) == before);
	assert(find_override(config, "DP-1") != NULL);
	assert(fixture.diagnostic_count == 1);

	snapshot.monitor_count = 2;
	assert(monitor_scale_config_match_override(
		   config, "DP-1", &snapshot, &factor)
	       == MONITOR_SCALE_MATCH_UNIQUE);
	assert(factor.quarters == 7);

	facts[0].name = "DP-1";
	before	      = config_digest(config);
	assert(monitor_scale_config_match_override(
		   config, "DP-1", &snapshot, &factor)
	       == MONITOR_SCALE_MATCH_AMBIGUOUS);
	assert(config_digest(config) == before);
	assert(fixture.diagnostic_count == 2);

	monitor_scale_config_shutdown();
	assert(fixture.live_allocations == 0);
}

static void
run_scale_command(void (*command)(F_CMD_ARGS), char *action)
{
	command(NULL, NULL, action, NULL);
}

static void
test_command_callbacks_enforce_exact_arity_and_canonical_factors(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	char				  valid_override[] = "DP-1 1.25";
	char				  valid_fallback[] = "1.75";
	char				  missing_name[]   = "";
	char				  missing_factor[] = "DP-1";
	char	 extra_override[]   = "DP-1 2 extra another";
	char	 missing_fallback[] = "";
	char	 extra_fallback[]   = "2 extra another";
	char	 unsupported[]	    = "DP-1 4.25";
	char	 alternate[]	    = "DP-1 2.50";
	uint64_t before;

	run_scale_command(CMD_MonitorScale, valid_override);
	assert(find_override(config, "DP-1")->factor.quarters == 5);
	run_scale_command(CMD_MonitorScaleFallback, valid_fallback);
	assert(config->has_fallback && config->fallback.quarters == 7);

	before = config_digest(config);
	run_scale_command(CMD_MonitorScale, missing_name);
	assert(config_digest(config) == before);
	before = config_digest(config);
	run_scale_command(CMD_MonitorScale, missing_factor);
	assert(config_digest(config) == before);
	before = config_digest(config);
	run_scale_command(CMD_MonitorScale, extra_override);
	assert(config_digest(config) == before);
	before = config_digest(config);
	run_scale_command(CMD_MonitorScaleFallback, missing_fallback);
	assert(config_digest(config) == before);
	before = config_digest(config);
	run_scale_command(CMD_MonitorScaleFallback, extra_fallback);
	assert(config_digest(config) == before);
	before = config_digest(config);
	run_scale_command(CMD_MonitorScale, unsupported);
	assert(config_digest(config) == before);
	before = config_digest(config);
	run_scale_command(CMD_MonitorScale, alternate);
	assert(config_digest(config) == before);

	monitor_scale_config_shutdown();
	assert(fixture.live_allocations == 0);
}

static void
test_command_callbacks_retain_alias_control_and_long_name_bytes(void)
{
	monitor_scale_config_test_fixture fixture = {0};
	monitor_scale_config		 *config = initialize_fixture(&fixture);
	char				  numeric_name[]  = "0 1.25";
	char				  global_alias[]  = "g 1.5";
	char				  current_alias[] = "c 1.75";
	char				  primary_alias[] = "p 2";
	char	 control_name[] = "\"line\nbreak\" invalid";
	char	 long_name[MAX_TOKEN_LENGTH + 32];
	uint64_t before;
	size_t	 i;

	run_scale_command(CMD_MonitorScale, numeric_name);
	run_scale_command(CMD_MonitorScale, global_alias);
	run_scale_command(CMD_MonitorScale, current_alias);
	run_scale_command(CMD_MonitorScale, primary_alias);
	assert(find_override(config, "0")->factor.quarters == 5);
	assert(find_override(config, "g")->factor.quarters == 6);
	assert(find_override(config, "c")->factor.quarters == 7);
	assert(find_override(config, "p")->factor.quarters == 8);

	before = config_digest(config);
	run_scale_command(CMD_MonitorScale, control_name);
	assert(config_digest(config) == before);
	assert(strstr(fixture.reason, "MonitorScale") != NULL);
	assert(strcmp(fixture.name, "line\\x0abreak") == 0);

	for (i = 0; i < MAX_TOKEN_LENGTH + 16; i++) {
		long_name[i] = 'A';
	}
	(void)snprintf(long_name + i, sizeof(long_name) - i, " 1.25");
	before = config_digest(config);
	run_scale_command(CMD_MonitorScale, long_name);
	assert(config_digest(config) == before);
	assert(strstr(fixture.reason, "long") != NULL);

	monitor_scale_config_shutdown();
	assert(fixture.live_allocations == 0);
}

int
main(void)
{
	test_valid_sequential_declarations_and_exact_matching();
	test_invalid_and_allocation_failed_replacements_retain_store();
	test_fallback_replacement_is_independent_and_transactional();
	test_zero_unique_and_ambiguous_active_match_outcomes();
	test_command_callbacks_enforce_exact_arity_and_canonical_factors();
	test_command_callbacks_retain_alias_control_and_long_name_bytes();
	test_association_resolves_every_source_and_retains_rejection_metadata();
	test_association_rejects_ambiguous_and_retains_unmatched_overrides();
	test_association_preserves_automatic_endpoint_reasons();
	test_recompute_is_allocation_free_total_and_idempotent();
	test_finalize_owns_one_store_and_rejects_late_mutation();
	test_print_info_emits_authoritative_reason_complete_escaped_output();
	return 0;
}
