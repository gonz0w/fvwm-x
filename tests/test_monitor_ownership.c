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
#include <string.h>

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
	    .name_atom	    = 99,
	    .name	    = "garbage",
	    .reason	    = MONITOR_OWNERSHIP_REASON_GAP,
	    .extra_overlap  = 123,
	};
	monitor_ownership_status status;

	status = monitor_ownership_resolve(&window, monitors, 2, None, 0,
	    &result);

	assert(status == MONITOR_OWNERSHIP_OK);
	assert(result.name_atom == 1);
	assert(result.name != NULL);
	assert(strcmp(result.name, "HDMI-1") == 0);
	assert(result.reason == MONITOR_OWNERSHIP_REASON_CONTAINMENT);
}

int
main(void)
{
	test_negative_origin_monitor_owns_contained_window();

	return 0;
}
