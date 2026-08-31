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

#include "libs/monitor_ownership.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

static monitor_ownership_status
resolve_owner(const rectangle *window,
    const monitor_topology_monitor *monitors, int monitor_count,
    monitor_ownership_result *result)
{
	return monitor_ownership_resolve(
	    window, monitors, monitor_count, None, 0, result);
}

static void
assert_owner(const monitor_ownership_result *result, Atom name_atom,
    const char *name, monitor_ownership_reason reason)
{
	assert(result->name_atom == name_atom);
	assert(result->name != NULL);
	assert(strcmp(result->name, name) == 0);
	assert(result->reason == reason);
}

static void
assert_safe_init(const monitor_ownership_result *result)
{
	assert(result->name_atom == None);
	assert(result->name == NULL);
	assert(result->extra_overlap == 0);
}

static void
test_negative_origin_monitor_owns_contained_window(void)
{
	monitor_topology_monitor monitors[] = {
	    {
		.name_atom = 1,
		.name	   = "HDMI-1",
		.x	   = -1920,
		.y	   = 0,
		.width	   = 1920,
		.height	   = 1080,
	    },
	    {
		.name_atom = 2,
		.name	   = "eDP-1",
		.x	   = 0,
		.y	   = 0,
		.width	   = 1920,
		.height	   = 1080,
		.primary   = True,
	    },
	};
	rectangle		 window = {
	    .x	    = -1800,
	    .y	    = 100,
	    .width  = 800,
	    .height = 600,
	};
	monitor_ownership_result result = {
	    .name_atom	   = 99,
	    .name	   = "garbage",
	    .reason	   = MONITOR_OWNERSHIP_REASON_GAP,
	    .extra_overlap = 123,
	};
	monitor_ownership_status status;

	status = resolve_owner(&window, monitors, 2, &result);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(&result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_CONTAINMENT);
}

static void
test_partial_overlap_unique_max_wins(void)
{
	monitor_topology_monitor monitors[] = {
	    {
		.name_atom = 1,
		.name	   = "HDMI-1",
		.x	   = 0,
		.y	   = 0,
		.width	   = 150,
		.height	   = 100,
	    },
	    {
		.name_atom = 2,
		.name	   = "DP-1",
		.x	   = 160,
		.y	   = 0,
		.width	   = 200,
		.height	   = 100,
	    },
	};
	rectangle		 window = {
	    .x	    = 50,
	    .y	    = 0,
	    .width  = 150,
	    .height = 100,
	};
	monitor_ownership_result result;
	monitor_ownership_status status;

	status = resolve_owner(&window, monitors, 2, &result);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert(result.name_atom == 1);
	assert(result.name != NULL);
	assert(strcmp(result.name, "HDMI-1") == 0);
	assert(result.reason == MONITOR_OWNERSHIP_REASON_OVERLAP
	       || result.reason == MONITOR_OWNERSHIP_REASON_CONTAINMENT);
}

static void
test_exact_clones_identity_tie_independent_of_order(void)
{
	monitor_topology_monitor ab[] = {
	    {
		.name_atom = 2,
		.name	   = "DP-1",
		.x	   = 0,
		.y	   = 0,
		.width	   = 1920,
		.height	   = 1080,
	    },
	    {
		.name_atom = 1,
		.name	   = "HDMI-1",
		.x	   = 0,
		.y	   = 0,
		.width	   = 1920,
		.height	   = 1080,
	    },
	};
	monitor_topology_monitor ba[] = {
	    ab[1],
	    ab[0],
	};
	rectangle		 window = {
	    .x	    = 100,
	    .y	    = 100,
	    .width  = 400,
	    .height = 300,
	};
	monitor_ownership_result result_ab;
	monitor_ownership_result result_ba;
	monitor_ownership_status status_ab;
	monitor_ownership_status status_ba;

	status_ab = resolve_owner(&window, ab, 2, &result_ab);
	status_ba = resolve_owner(&window, ba, 2, &result_ba);

	assert(status_ab == MONITOR_OWNERSHIP_OK);
	assert(status_ba == MONITOR_OWNERSHIP_OK);
	assert_owner(&result_ab, 1, "HDMI-1", result_ab.reason);
	assert_owner(&result_ba, 1, "HDMI-1", result_ba.reason);
	assert(result_ab.name_atom == result_ba.name_atom);
}

static void
test_output_reorder_preserves_owner(void)
{
	monitor_topology_monitor ab[] = {
	    {
		.name_atom = 1,
		.name	   = "HDMI-1",
		.x	   = -1920,
		.y	   = 0,
		.width	   = 1920,
		.height	   = 1080,
	    },
	    {
		.name_atom = 2,
		.name	   = "DP-1",
		.x	   = 0,
		.y	   = 0,
		.width	   = 1920,
		.height	   = 1080,
	    },
	};
	monitor_topology_monitor ba[] = {
	    ab[1],
	    ab[0],
	};
	rectangle		 window = {
	    .x	    = -1600,
	    .y	    = 40,
	    .width  = 800,
	    .height = 600,
	};
	monitor_ownership_result result_ab;
	monitor_ownership_result result_ba;

	assert(resolve_owner(&window, ab, 2, &result_ab)
	       == MONITOR_OWNERSHIP_OK);
	assert(resolve_owner(&window, ba, 2, &result_ba)
	       == MONITOR_OWNERSHIP_OK);
	assert_owner(
	    &result_ab, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_CONTAINMENT);
	assert_owner(
	    &result_ba, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_CONTAINMENT);
}

static void
test_rotation_swapped_extents_still_contain(void)
{
	monitor_topology_monitor monitors[] = {
	    {
		.name_atom = 1,
		.name	   = "HDMI-1",
		.x	   = 0,
		.y	   = 0,
		.width	   = 1080,
		.height	   = 1920,
	    },
	    {
		.name_atom = 2,
		.name	   = "DP-1",
		.x	   = 1080,
		.y	   = 0,
		.width	   = 1920,
		.height	   = 1080,
	    },
	};
	rectangle		 window = {
	    .x	    = 40,
	    .y	    = 100,
	    .width  = 800,
	    .height = 1600,
	};
	monitor_ownership_result result;

	assert(resolve_owner(&window, monitors, 2, &result)
	       == MONITOR_OWNERSHIP_OK);
	assert_owner(
	    &result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_CONTAINMENT);
}

static void
test_gap_uses_manhattan_then_identity(void)
{
	monitor_topology_monitor monitors[] = {
	    {
		.name_atom = 2,
		.name	   = "DP-1",
		.x	   = 100,
		.y	   = 0,
		.width	   = 1920,
		.height	   = 1080,
	    },
	    {
		.name_atom = 1,
		.name	   = "HDMI-1",
		.x	   = -1920,
		.y	   = 0,
		.width	   = 1920,
		.height	   = 1080,
	    },
	};
	rectangle		 window = {
	    .x	    = 20,
	    .y	    = 100,
	    .width  = 10,
	    .height = 10,
	};
	monitor_ownership_result result;

	assert(resolve_owner(&window, monitors, 2, &result)
	       == MONITOR_OWNERSHIP_OK);
	assert_owner(&result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_GAP);
}

static void
test_empty_input_initialized_fallback(void)
{
	rectangle		 window = {
	    .x	    = 10,
	    .y	    = 10,
	    .width  = 20,
	    .height = 20,
	};
	monitor_ownership_result result = {
	    .name_atom	   = 99,
	    .name	   = "garbage",
	    .reason	   = MONITOR_OWNERSHIP_REASON_GAP,
	    .extra_overlap = 123,
	};
	monitor_ownership_status status;

	status = resolve_owner(&window, NULL, 0, &result);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert(result.reason == MONITOR_OWNERSHIP_REASON_FALLBACK);
	assert_safe_init(&result);
}

static void
test_invalid_arguments_initialize_outputs(void)
{
	rectangle		 window = {
	    .x	    = 0,
	    .y	    = 0,
	    .width  = 10,
	    .height = 10,
	};
	monitor_topology_monitor monitors[] = {
	    {
		.name_atom = 1,
		.name	   = "HDMI-1",
		.x	   = 0,
		.y	   = 0,
		.width	   = 100,
		.height	   = 100,
	    },
	};
	monitor_ownership_result result = {
	    .name_atom	   = 99,
	    .name	   = "garbage",
	    .reason	   = MONITOR_OWNERSHIP_REASON_GAP,
	    .extra_overlap = 123,
	};

	assert(resolve_owner(NULL, monitors, 1, &result)
	       == MONITOR_OWNERSHIP_INVALID_ARGUMENT);
	assert_safe_init(&result);
	assert(result.reason == MONITOR_OWNERSHIP_REASON_FALLBACK);

	result = (monitor_ownership_result){
	    .name_atom	   = 99,
	    .name	   = "garbage",
	    .reason	   = MONITOR_OWNERSHIP_REASON_GAP,
	    .extra_overlap = 123,
	};
	assert(resolve_owner(&window, monitors, -1, &result)
	       == MONITOR_OWNERSHIP_INVALID_ARGUMENT);
	assert_safe_init(&result);

	assert(monitor_ownership_resolve(
		   &window, monitors, 1, None, 0, NULL)
	       == MONITOR_OWNERSHIP_INVALID_ARGUMENT);
}

static void
test_overlap_area_overflow_initializes_outputs(void)
{
	monitor_topology_monitor monitors[] = {
	    {
		.name_atom = 1,
		.name	   = "HDMI-1",
		.x	   = 0,
		.y	   = 0,
		.width	   = INT_MAX,
		.height	   = 2,
	    },
	};
	rectangle		 window = {
	    .x	    = 0,
	    .y	    = 0,
	    .width  = INT_MAX,
	    .height = 2,
	};
	monitor_ownership_result result = {
	    .name_atom	   = 99,
	    .name	   = "garbage",
	    .reason	   = MONITOR_OWNERSHIP_REASON_GAP,
	    .extra_overlap = 123,
	};

	assert(resolve_owner(&window, monitors, 1, &result)
	       == MONITOR_OWNERSHIP_OVERFLOW);
	assert_safe_init(&result);
}

static void
fill_seam_monitors(monitor_topology_monitor monitors[2])
{
	monitors[0] = (monitor_topology_monitor){
	    .name_atom = 1,
	    .name      = "HDMI-1",
	    .x	       = 0,
	    .y	       = 0,
	    .width     = 1000,
	    .height    = 1000,
	};
	monitors[1] = (monitor_topology_monitor){
	    .name_atom = 2,
	    .name      = "DP-1",
	    .x	       = 1000,
	    .y	       = 0,
	    .width     = 1000,
	    .height    = 1000,
	};
}

static void
test_incumbent_kept_on_equal_seam(void)
{
	monitor_topology_monitor monitors[2];
	rectangle		 window = {
	    .x	    = 900,
	    .y	    = 0,
	    .width  = 200,
	    .height = 100,
	};
	monitor_ownership_result result;
	monitor_ownership_status status;

	fill_seam_monitors(monitors);

	status = monitor_ownership_resolve(&window, monitors, 2, 1,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(
	    &result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_INCUMBENT);
	assert(result.extra_overlap == 0);

	status = monitor_ownership_resolve(&window, monitors, 2, 2,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(&result, 2, "DP-1", MONITOR_OWNERSHIP_REASON_INCUMBENT);
	assert(result.extra_overlap == 0);
}

static void
test_incumbent_kept_when_extra_overlap_equals_hysteresis(void)
{
	monitor_topology_monitor monitors[2];
	/*
	 * 100-wide strip along the x=1000 seam: height 1 at x=954 overlaps
	 * A by 46 and B by 54, so extra_overlap is 8.
	 */
	rectangle		 window = {
	    .x	    = 954,
	    .y	    = 0,
	    .width  = 100,
	    .height = 1,
	};
	monitor_ownership_result result;
	monitor_ownership_status status;

	fill_seam_monitors(monitors);

	status = monitor_ownership_resolve(&window, monitors, 2, 1,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(
	    &result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_INCUMBENT);
	assert(result.extra_overlap == MONITOR_OWNERSHIP_HYSTERESIS_PX);
	assert(result.extra_overlap == 8);
}

static void
test_challenger_wins_beyond_hysteresis(void)
{
	monitor_topology_monitor monitors[2];
	rectangle		 strip = {
	    .x	    = 954,
	    .y	    = 0,
	    .width  = 101,
	    .height = 1,
	};
	rectangle		 window = {
	    .x	    = 891,
	    .y	    = 0,
	    .width  = 200,
	    .height = 100,
	};
	monitor_ownership_result result;
	monitor_ownership_status status;

	fill_seam_monitors(monitors);

	status = monitor_ownership_resolve(&strip, monitors, 2, 1,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(&result, 2, "DP-1", MONITOR_OWNERSHIP_REASON_OVERLAP);
	assert(result.extra_overlap == 9);

	status = monitor_ownership_resolve(&window, monitors, 2, 2,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(&result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_OVERLAP);
	assert(result.extra_overlap == 1800);
}

static void
test_incumbent_kept_when_one_pixel_overlap_remains(void)
{
	monitor_topology_monitor monitors[2];
	/*
	 * Incumbent overlap area is exactly 1 (x=999..1000) while the
	 * challenger has 9, so extra_overlap is 8 — at most hysteresis.
	 */
	rectangle		 window = {
	    .x	    = 999,
	    .y	    = 0,
	    .width  = 10,
	    .height = 1,
	};
	monitor_ownership_result result;
	monitor_ownership_status status;

	fill_seam_monitors(monitors);

	status = monitor_ownership_resolve(&window, monitors, 2, 1,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(
	    &result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_INCUMBENT);
	assert(result.extra_overlap <= MONITOR_OWNERSHIP_HYSTERESIS_PX);
	assert(result.extra_overlap == 8);
}

static void
test_work_rect_negative_origin_struts(void)
{
	rectangle monitor_rect = {
	    .x	    = -1920,
	    .y	    = 0,
	    .width  = 1920,
	    .height = 1080,
	};
	rectangle		 work = {
	    .x	    = 99,
	    .y	    = 99,
	    .width  = 99,
	    .height = 99,
	};
	monitor_ownership_status status;

	status = monitor_ownership_work_rect(
	    &monitor_rect, 64, 0, 32, 0, &work);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert(work.x == -1856);
	assert(work.y == 32);
	assert(work.width == 1856);
	assert(work.height == 1048);
}

static void
test_work_rect_invalid_insets_initialize_outputs(void)
{
	rectangle monitor_rect = {
	    .x	    = -1920,
	    .y	    = 0,
	    .width  = 1920,
	    .height = 1080,
	};
	rectangle work = {
	    .x	    = 99,
	    .y	    = 99,
	    .width  = 99,
	    .height = 99,
	};

	assert(monitor_ownership_work_rect(
		   &monitor_rect, 2000, 0, 0, 0, &work)
	       == MONITOR_OWNERSHIP_INVALID_ARGUMENT);
	assert(work.x == 0);
	assert(work.y == 0);
	assert(work.width == 0);
	assert(work.height == 0);

	work = (rectangle){
	    .x	    = 99,
	    .y	    = 99,
	    .width  = 99,
	    .height = 99,
	};
	assert(monitor_ownership_work_rect(NULL, 0, 0, 0, 0, &work)
	       == MONITOR_OWNERSHIP_INVALID_ARGUMENT);
	assert(work.x == 0);
	assert(work.y == 0);
	assert(work.width == 0);
	assert(work.height == 0);

	assert(monitor_ownership_work_rect(
		   &monitor_rect, 0, 0, 0, 0, NULL)
	       == MONITOR_OWNERSHIP_INVALID_ARGUMENT);
}

static void
test_work_insets_negative_origin(void)
{
	rectangle monitor_rect = {
	    .x	    = -1920,
	    .y	    = 0,
	    .width  = 1920,
	    .height = 1080,
	};
	rectangle		 work;
	boundingbox		 box = {
	    .left   = 99,
	    .right  = 99,
	    .top    = 99,
	    .bottom = 99,
	};
	monitor_ownership_status status;

	status = monitor_ownership_work_rect(
	    &monitor_rect, 64, 0, 32, 0, &work);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert(work.x == -1856);
	assert(work.y == 32);
	assert(work.width == 1856);
	assert(work.height == 1048);

	status = monitor_ownership_work_insets(&monitor_rect, &work, &box);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert(box.left == 64);
	assert(box.right == 0);
	assert(box.top == 32);
	assert(box.bottom == 0);
}

static void
test_work_insets_right_hand_origin(void)
{
	rectangle monitor_rect = {
	    .x	    = 1920,
	    .y	    = 0,
	    .width  = 1920,
	    .height = 1080,
	};
	rectangle		 work;
	boundingbox		 box;
	monitor_ownership_status status;

	status = monitor_ownership_work_rect(
	    &monitor_rect, 64, 0, 32, 0, &work);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert(work.x == 1984);
	assert(work.y == 32);
	assert(work.width == 1856);
	assert(work.height == 1048);

	status = monitor_ownership_work_insets(&monitor_rect, &work, &box);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert(box.left == 64);
	assert(box.right == 0);
	assert(box.top == 32);
	assert(box.bottom == 0);
}

static void
test_work_rect_paged_negative_origin(void)
{
	rectangle monitor_rect = {
	    .x	    = -1920,
	    .y	    = 0,
	    .width  = 1920,
	    .height = 1080,
	};
	rectangle		 work;
	rectangle		 paged;
	monitor_ownership_status status;

	status = monitor_ownership_work_rect(
	    &monitor_rect, 64, 0, 32, 0, &work);
	assert(status == MONITOR_OWNERSHIP_OK);

	status = monitor_ownership_work_rect_paged(&work, 0, 0, &paged);
	assert(status == MONITOR_OWNERSHIP_OK);
	assert(paged.x == -1856);
	assert(paged.y == 32);
	assert(paged.width == 1856);
	assert(paged.height == 1048);

	status = monitor_ownership_work_rect_paged(&work, 1920, 0, &paged);
	assert(status == MONITOR_OWNERSHIP_OK);
	assert(paged.x == 64);
	assert(paged.y == 32);
	assert(paged.width == 1856);
	assert(paged.height == 1048);
}

static void
test_minoverlap_edges_use_insets(void)
{
	rectangle monitor_rect = {
	    .x	    = -1920,
	    .y	    = 0,
	    .width  = 1920,
	    .height = 1080,
	};
	rectangle		 work;
	boundingbox		 box;
	int			 page_p1_x;
	int			 xtest;

	assert(monitor_ownership_work_rect(
		   &monitor_rect, 64, 0, 32, 0, &work)
	       == MONITOR_OWNERSHIP_OK);
	assert(monitor_ownership_work_insets(&monitor_rect, &work, &box)
	       == MONITOR_OWNERSHIP_OK);
	assert(box.left == 64);

	page_p1_x = -1920;
	xtest	  = page_p1_x + box.left;
	assert(xtest == -1856);
	assert(xtest == work.x);

	page_p1_x = 1920;
	xtest	  = page_p1_x + box.left;
	assert(xtest == 1984);
}

static void
test_work_insets_null_args_initialize_outputs(void)
{
	rectangle monitor_rect = {
	    .x	    = -1920,
	    .y	    = 0,
	    .width  = 1920,
	    .height = 1080,
	};
	rectangle work = {
	    .x	    = -1856,
	    .y	    = 32,
	    .width  = 1856,
	    .height = 1048,
	};
	boundingbox box = {
	    .left   = 99,
	    .right  = 99,
	    .top    = 99,
	    .bottom = 99,
	};

	assert(monitor_ownership_work_insets(NULL, &work, &box)
	       == MONITOR_OWNERSHIP_INVALID_ARGUMENT);
	assert(box.left == 0);
	assert(box.right == 0);
	assert(box.top == 0);
	assert(box.bottom == 0);

	box = (boundingbox){
	    .left   = 99,
	    .right  = 99,
	    .top    = 99,
	    .bottom = 99,
	};
	assert(monitor_ownership_work_insets(&monitor_rect, NULL, &box)
	       == MONITOR_OWNERSHIP_INVALID_ARGUMENT);
	assert(box.left == 0);
	assert(box.right == 0);
	assert(box.top == 0);
	assert(box.bottom == 0);

	assert(monitor_ownership_work_insets(&monitor_rect, &work, NULL)
	       == MONITOR_OWNERSHIP_INVALID_ARGUMENT);
}

static void
test_work_rect_paged_overflow_initializes_outputs(void)
{
	rectangle work = {
	    .x	    = INT_MAX,
	    .y	    = 32,
	    .width  = 1856,
	    .height = 1048,
	};
	rectangle paged = {
	    .x	    = 99,
	    .y	    = 99,
	    .width  = 99,
	    .height = 99,
	};

	assert(monitor_ownership_work_rect_paged(&work, 1, 0, &paged)
	       == MONITOR_OWNERSHIP_OVERFLOW);
	assert(paged.x == 0);
	assert(paged.y == 0);
	assert(paged.width == 0);
	assert(paged.height == 0);

	paged = (rectangle){
	    .x	    = 99,
	    .y	    = 99,
	    .width  = 99,
	    .height = 99,
	};
	assert(monitor_ownership_work_rect_paged(NULL, 0, 0, &paged)
	       == MONITOR_OWNERSHIP_INVALID_ARGUMENT);
	assert(paged.x == 0);
	assert(paged.y == 0);
	assert(paged.width == 0);
	assert(paged.height == 0);

	assert(monitor_ownership_work_rect_paged(&work, 0, 0, NULL)
	       == MONITOR_OWNERSHIP_INVALID_ARGUMENT);
}

static void
test_incumbent_ignored_when_it_no_longer_overlaps(void)
{
	monitor_topology_monitor monitors[2];
	rectangle		 window = {
	    .x	    = 100,
	    .y	    = 100,
	    .width  = 200,
	    .height = 100,
	};
	monitor_ownership_result result;
	monitor_ownership_status status;

	fill_seam_monitors(monitors);

	status = monitor_ownership_resolve(&window, monitors, 2, 2,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert_owner(
	    &result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_CONTAINMENT);
	assert(result.reason != MONITOR_OWNERSHIP_REASON_INCUMBENT);
}

int
main(void)
{
	test_negative_origin_monitor_owns_contained_window();
	test_partial_overlap_unique_max_wins();
	test_exact_clones_identity_tie_independent_of_order();
	test_output_reorder_preserves_owner();
	test_rotation_swapped_extents_still_contain();
	test_gap_uses_manhattan_then_identity();
	test_empty_input_initialized_fallback();
	test_invalid_arguments_initialize_outputs();
	test_overlap_area_overflow_initializes_outputs();
	test_incumbent_kept_on_equal_seam();
	test_incumbent_kept_when_extra_overlap_equals_hysteresis();
	test_challenger_wins_beyond_hysteresis();
	test_incumbent_kept_when_one_pixel_overlap_remains();
	test_incumbent_ignored_when_it_no_longer_overlaps();
	test_work_rect_negative_origin_struts();
	test_work_rect_invalid_insets_initialize_outputs();
	test_work_insets_negative_origin();
	test_work_insets_right_hand_origin();
	test_work_rect_paged_negative_origin();
	test_minoverlap_edges_use_insets();
	test_work_insets_null_args_initialize_outputs();
	test_work_rect_paged_overflow_initializes_outputs();

	return 0;
}
