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

	fixture->diagnostic_count++;
	(void)snprintf(fixture->reason, sizeof(fixture->reason), "%s",
	    reason == NULL ? "" : reason);
	(void)snprintf(fixture->name, sizeof(fixture->name), "%s",
	    name == NULL ? "" : name);
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
	assert(strcmp(fixture.name, "line\nbreak") == 0);

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
	return 0;
}
