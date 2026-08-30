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

#include "fvwm/status_json.h"
#include "libs/cJSON.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define RETAINED_MONITOR_COUNT 1025

static bool
desk_is_urgent(struct monitor *monitor, int desk)
{
	(void)monitor;
	(void)desk;
	return false;
}

static int
desk_client_count(struct monitor *monitor, int desk)
{
	(void)monitor;
	(void)desk;
	return 0;
}

static monitor_topology_snapshot
make_topology(monitor_topology_monitor *monitor)
{
	*monitor = (monitor_topology_monitor){
	    .name_atom = 1,
	    .name      = "topology-monitor",
	    .width     = 1,
	    .height    = 1,
	    .primary   = True,
	};
	return (monitor_topology_snapshot){
	    .monitors	   = monitor,
	    .monitor_count = 1,
	    .epoch	   = 1,
	};
}

static void
test_more_than_1024_retained_monitors_are_serialized(void)
{
	struct monitors legacy_monitors;
	struct monitor *monitors =
	    calloc(RETAINED_MONITOR_COUNT, sizeof(*monitors));
	struct screen_info *screens =
	    calloc(RETAINED_MONITOR_COUNT, sizeof(*screens));
	DesktopsInfo *desktops =
	    calloc(RETAINED_MONITOR_COUNT, sizeof(*desktops));
	char **names = calloc(RETAINED_MONITOR_COUNT, sizeof(*names));
	monitor_topology_monitor  topology_monitor;
	monitor_topology_snapshot topology = make_topology(&topology_monitor);
	cJSON			 *message;
	cJSON			 *encoded_screens;
	cJSON			 *screen;
	size_t			  index;
	size_t			  encoded_count = 0;

	assert(monitors != NULL);
	assert(screens != NULL);
	assert(desktops != NULL);
	assert(names != NULL);
	RB_INIT(&legacy_monitors);
	for (index = 0; index < RETAINED_MONITOR_COUNT; index++) {
		names[index] = malloc(32);
		assert(names[index] != NULL);
		(void)snprintf(names[index], 32, "retained-%04zu", index);
		monitors[index].si	 = &screens[index];
		monitors[index].si->name = names[index];
		monitors[index].si->x	 = (int)index;
		monitors[index].si->w	 = 1;
		monitors[index].si->h	 = 1;
		monitors[index].Desktops = &desktops[index];
		monitors[index].number	 = (int)index;
		assert(RB_INSERT(monitors, &legacy_monitors, &monitors[index])
		       == NULL);
	}

	message = fvwm_status_json_create("retained-0000", "global", &topology,
	    &legacy_monitors, NULL, desk_is_urgent, desk_client_count);
	assert(message != NULL);
	encoded_screens = cJSON_GetObjectItemCaseSensitive(message, "screens");
	assert(cJSON_IsObject(encoded_screens));
	cJSON_ArrayForEach(screen, encoded_screens)
	{
		encoded_count++;
	}
	assert(encoded_count == RETAINED_MONITOR_COUNT);

	cJSON_Delete(message);
	for (index = 0; index < RETAINED_MONITOR_COUNT; index++) {
		free(names[index]);
	}
	free(names);
	free(desktops);
	free(screens);
	free(monitors);
}

int
main(void)
{
	test_more_than_1024_retained_monitors_are_serialized();
	return 0;
}
