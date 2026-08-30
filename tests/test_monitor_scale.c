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

#include "libs/monitor_scale.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

static monitor_topology_monitor
monitor_with_exact_dpi(int dpi_x, int dpi_y)
{
	monitor_topology_monitor monitor = {
	    .width     = dpi_x * 5,
	    .height    = dpi_y * 5,
	    .mm_width  = 127,
	    .mm_height = 127,
	};

	return monitor;
}

static void
test_valid_committed_dpi_resolves_automatic_factor(void)
{
	monitor_topology_monitor monitor = {
	    .width     = 1920,
	    .height    = 1080,
	    .mm_width  = 254,
	    .mm_height = 143,
	};
	monitor_scale_result result;

	result = monitor_scale_resolve(&monitor, NULL, NULL);

	assert(result.factor.quarters == 8);
	assert(result.source == MONITOR_SCALE_SOURCE_AUTOMATIC);
	assert(result.auto_status == MONITOR_SCALE_AUTO_VALID);
	assert(result.boundary == MONITOR_SCALE_BOUNDARY_NONE);
	assert(result.reason == MONITOR_SCALE_REASON_VALID_AUTOMATIC);
}

static void
test_all_canonical_factors_parse_and_format(void)
{
	static const char *const canonical[] = {
	    "1",
	    "1.25",
	    "1.5",
	    "1.75",
	    "2",
	    "2.25",
	    "2.5",
	    "2.75",
	    "3",
	    "3.25",
	    "3.5",
	    "3.75",
	    "4",
	};
	size_t i;

	for (i = 0; i < sizeof(canonical) / sizeof(canonical[0]); i++) {
		monitor_scale_factor factor    = {99};
		char		     buffer[5] = "bad";

		assert(monitor_scale_parse_factor(canonical[i], &factor));
		assert(factor.quarters == MONITOR_SCALE_QUARTERS_MIN + i);
		assert(monitor_scale_format_factor(
		    factor, buffer, sizeof(buffer)));
		assert(strcmp(buffer, canonical[i]) == 0);
	}
}

static void
test_factor_parser_rejects_noncanonical_and_initializes_output(void)
{
	static const char *const invalid[] = {
	    "",
	    "0.75",
	    "4.25",
	    "1.0",
	    "1.00",
	    "1.50",
	    "+1",
	    "-1",
	    "01",
	    "2e0",
	    "125%",
	    "5/4",
	    " 1",
	    "1 ",
	    "1.2",
	    NULL,
	};
	size_t i;

	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		monitor_scale_factor factor = {99};

		assert(!monitor_scale_parse_factor(invalid[i], &factor));
		assert(factor.quarters == MONITOR_SCALE_QUARTERS_MIN);
	}
	assert(!monitor_scale_parse_factor("1", NULL));
}

static void
test_factor_formatter_rejects_invalid_or_short_output(void)
{
	char buffer[5] = "bad";

	assert(!monitor_scale_format_factor(
	    (monitor_scale_factor){3}, buffer, sizeof(buffer)));
	assert(buffer[0] == '\0');
	strcpy(buffer, "bad");
	assert(
	    !monitor_scale_format_factor((monitor_scale_factor){5}, buffer, 4));
	assert(buffer[0] == '\0');
	assert(!monitor_scale_format_factor(
	    (monitor_scale_factor){5}, NULL, sizeof(buffer)));
}

static void
test_dpi_axis_boundaries_are_exact_and_inclusive(void)
{
	monitor_topology_monitor monitor;
	monitor_scale_result	 result;

	monitor = monitor_with_exact_dpi(50, 50);
	result	= monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.auto_status == MONITOR_SCALE_AUTO_VALID);
	assert(result.factor.quarters == MONITOR_SCALE_QUARTERS_MIN);
	assert(result.boundary == MONITOR_SCALE_BOUNDARY_LOW);
	assert(result.reason == MONITOR_SCALE_REASON_AUTOMATIC_BOUND_LOW);

	monitor = monitor_with_exact_dpi(500, 500);
	result	= monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.auto_status == MONITOR_SCALE_AUTO_VALID);
	assert(result.factor.quarters == MONITOR_SCALE_QUARTERS_MAX);
	assert(result.boundary == MONITOR_SCALE_BOUNDARY_HIGH);
	assert(result.reason == MONITOR_SCALE_REASON_AUTOMATIC_BOUND_HIGH);

	monitor = monitor_with_exact_dpi(49, 50);
	result	= monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.auto_status == MONITOR_SCALE_AUTO_X_IMPLAUSIBLE);

	monitor = monitor_with_exact_dpi(50, 49);
	result	= monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.auto_status == MONITOR_SCALE_AUTO_Y_IMPLAUSIBLE);

	monitor = monitor_with_exact_dpi(501, 500);
	result	= monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.auto_status == MONITOR_SCALE_AUTO_X_IMPLAUSIBLE);

	monitor = monitor_with_exact_dpi(500, 501);
	result	= monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.auto_status == MONITOR_SCALE_AUTO_Y_IMPLAUSIBLE);
}

static void
test_dpi_agreement_boundary_is_symmetric_and_exact(void)
{
	monitor_topology_monitor monitor = monitor_with_exact_dpi(95, 105);
	monitor_scale_result	 result;

	result = monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.auto_status == MONITOR_SCALE_AUTO_VALID);

	monitor = monitor_with_exact_dpi(94, 105);
	result	= monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.auto_status == MONITOR_SCALE_AUTO_INCONSISTENT);

	monitor = monitor_with_exact_dpi(105, 94);
	result	= monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.auto_status == MONITOR_SCALE_AUTO_INCONSISTENT);
}

static void
test_every_bucket_midpoint_selects_the_higher_quarter(void)
{
	unsigned int lower;

	for (lower = MONITOR_SCALE_QUARTERS_MIN;
	    lower < MONITOR_SCALE_QUARTERS_MAX; lower++) {
		int			 midpoint_dpi = (int)(24 * lower + 12);
		monitor_topology_monitor monitor =
		    monitor_with_exact_dpi(midpoint_dpi, midpoint_dpi);
		monitor_scale_result result =
		    monitor_scale_resolve(&monitor, NULL, NULL);

		assert(result.auto_status == MONITOR_SCALE_AUTO_VALID);
		assert(result.factor.quarters == lower + 1);
	}
}

static void
test_missing_axes_and_extreme_facts_are_rejected_safely(void)
{
	monitor_topology_monitor monitor = monitor_with_exact_dpi(96, 96);
	monitor_scale_result	 result;

	monitor.mm_width = 0;
	result		 = monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.auto_status == MONITOR_SCALE_AUTO_MISSING_AXIS);
	assert(result.source == MONITOR_SCALE_SOURCE_SAFE);

	monitor = (monitor_topology_monitor){
	    .width     = INT_MAX,
	    .height    = INT_MAX,
	    .mm_width  = INT_MAX,
	    .mm_height = INT_MAX,
	};
	result = monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.auto_status == MONITOR_SCALE_AUTO_X_IMPLAUSIBLE);
}

static void
test_resolver_precedence_and_rejected_auto_metadata(void)
{
	monitor_topology_monitor valid	  = monitor_with_exact_dpi(192, 192);
	monitor_topology_monitor invalid  = monitor_with_exact_dpi(49, 49);
	monitor_scale_factor	 override = {7};
	monitor_scale_factor	 fallback = {6};
	monitor_scale_result	 result;

	result = monitor_scale_resolve(&valid, &override, &fallback);
	assert(result.factor.quarters == override.quarters);
	assert(result.source == MONITOR_SCALE_SOURCE_OVERRIDE);
	assert(result.match_status == MONITOR_SCALE_MATCH_UNIQUE);
	assert(result.reason == MONITOR_SCALE_REASON_EXACT_OVERRIDE);

	result = monitor_scale_resolve(&valid, NULL, &fallback);
	assert(result.factor.quarters == 8);
	assert(result.source == MONITOR_SCALE_SOURCE_AUTOMATIC);

	override.quarters = MONITOR_SCALE_QUARTERS_MIN - 1;
	result		  = monitor_scale_resolve(&valid, &override, &fallback);
	assert(result.factor.quarters == 8);
	assert(result.source == MONITOR_SCALE_SOURCE_AUTOMATIC);

	result = monitor_scale_resolve(&invalid, NULL, &fallback);
	assert(result.factor.quarters == fallback.quarters);
	assert(result.source == MONITOR_SCALE_SOURCE_FALLBACK);
	assert(result.auto_status == MONITOR_SCALE_AUTO_X_IMPLAUSIBLE);
	assert(result.reason == MONITOR_SCALE_REASON_CONFIGURED_FALLBACK);

	result = monitor_scale_resolve(&invalid, NULL, NULL);
	assert(result.factor.quarters == MONITOR_SCALE_QUARTERS_MIN);
	assert(result.source == MONITOR_SCALE_SOURCE_SAFE);
	assert(result.auto_status == MONITOR_SCALE_AUTO_X_IMPLAUSIBLE);
	assert(result.reason == MONITOR_SCALE_REASON_SAFE_DEFAULT);
}

static void
test_apply_and_remove_round_half_away_from_zero(void)
{
	int converted = 99;

	assert(monitor_scale_apply(1, (monitor_scale_factor){6}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 2);
	assert(monitor_scale_apply(0, (monitor_scale_factor){16}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 0);
	assert(monitor_scale_apply(-1, (monitor_scale_factor){6}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == -2);
	assert(monitor_scale_apply(3, (monitor_scale_factor){6}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 5);
	assert(monitor_scale_apply(1, (monitor_scale_factor){5}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 1);
	assert(monitor_scale_apply(2, (monitor_scale_factor){5}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 3);
	assert(monitor_scale_apply(-2, (monitor_scale_factor){5}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == -3);
	assert(monitor_scale_apply(-3, (monitor_scale_factor){5}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == -4);

	assert(monitor_scale_remove(1, (monitor_scale_factor){8}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 1);
	assert(monitor_scale_remove(-1, (monitor_scale_factor){8}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == -1);
	assert(monitor_scale_remove(3, (monitor_scale_factor){8}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 2);
}

static void
test_conversion_limits_overflow_and_invalid_outputs(void)
{
	int converted = 99;

	assert(
	    monitor_scale_apply(INT_MAX, (monitor_scale_factor){4}, &converted)
	    == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == INT_MAX);
	assert(
	    monitor_scale_apply(INT_MIN, (monitor_scale_factor){4}, &converted)
	    == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == INT_MIN);
	assert(
	    monitor_scale_remove(INT_MAX, (monitor_scale_factor){4}, &converted)
	    == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == INT_MAX);
	assert(
	    monitor_scale_remove(INT_MIN, (monitor_scale_factor){4}, &converted)
	    == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == INT_MIN);

	assert(
	    monitor_scale_apply(INT_MAX, (monitor_scale_factor){5}, &converted)
	    == MONITOR_SCALE_CONVERSION_OVERFLOW);
	assert(converted == 0);
	converted = 99;
	assert(
	    monitor_scale_apply(INT_MIN, (monitor_scale_factor){5}, &converted)
	    == MONITOR_SCALE_CONVERSION_OVERFLOW);
	assert(converted == 0);

	converted = 99;
	assert(monitor_scale_apply(1, (monitor_scale_factor){3}, &converted)
	       == MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT);
	assert(converted == 0);
	assert(monitor_scale_remove(1, (monitor_scale_factor){17}, &converted)
	       == MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT);
	assert(converted == 0);
	assert(monitor_scale_apply(1, (monitor_scale_factor){4}, NULL)
	       == MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT);
}

static void
test_repeated_conversions_start_from_the_original_source(void)
{
	monitor_scale_factor factor = {5};
	int		     iteration;

	for (iteration = 0; iteration < 1000; iteration++) {
		int converted = 0;

		assert(monitor_scale_apply(7, factor, &converted)
		       == MONITOR_SCALE_CONVERSION_OK);
		assert(converted == 9);
	}
}

int
main(void)
{
	test_valid_committed_dpi_resolves_automatic_factor();
	test_all_canonical_factors_parse_and_format();
	test_factor_parser_rejects_noncanonical_and_initializes_output();
	test_factor_formatter_rejects_invalid_or_short_output();
	test_dpi_axis_boundaries_are_exact_and_inclusive();
	test_dpi_agreement_boundary_is_symmetric_and_exact();
	test_every_bucket_midpoint_selects_the_higher_quarter();
	test_missing_axes_and_extreme_facts_are_rejected_safely();
	test_resolver_precedence_and_rejected_auto_metadata();
	test_apply_and_remove_round_half_away_from_zero();
	test_conversion_limits_overflow_and_invalid_outputs();
	test_repeated_conversions_start_from_the_original_source();

	return 0;
}
