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

int
main(void)
{
	test_valid_committed_dpi_resolves_automatic_factor();

	return 0;
}
