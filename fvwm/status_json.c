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

#include "status_json.h"

#include "fvwm.h"
#include "libs/cJSON.h"

static bool
status_json_add_screen(cJSON *screens, struct monitor *monitor,
    const FvwmWindow *focused_window, fvwm_status_desk_urgent_fn desk_is_urgent,
    fvwm_status_desk_count_fn desk_client_count)
{
	cJSON	     *screen = NULL;
	cJSON	     *desktops;
	DesktopsInfo *desktop;

	if (monitor->Desktops == NULL) {
		return false;
	}
	screen = cJSON_CreateObject();
	if (screen == NULL) {
		return false;
	}
	desktops = cJSON_AddObjectToObject(screen, "desktops");
	if (desktops == NULL) {
		goto fail;
	}
	for (desktop = monitor->Desktops->next; desktop != NULL;
	    desktop  = desktop->next) {
		cJSON *item = cJSON_CreateObject();

		if (item == NULL
		    || cJSON_AddNumberToObject(item, "number", desktop->desk)
			   == NULL
		    || cJSON_AddBoolToObject(item, "is_current",
			   desktop->desk == monitor->virtual_scr.CurrentDesk)
			   == NULL
		    || cJSON_AddBoolToObject(item, "is_urgent",
			   desk_is_urgent(monitor, desktop->desk))
			   == NULL
		    || cJSON_AddNumberToObject(item, "number_of_clients",
			   desk_client_count(monitor, desktop->desk))
			   == NULL
		    || !cJSON_AddItemToObject(desktops, desktop->name, item)) {
			cJSON_Delete(item);
			goto fail;
		}
	}
	if (focused_window != NULL && focused_window->m == monitor
	    && cJSON_AddStringToObject(
		   screen, "current_client", focused_window->visible_name)
		   == NULL) {
		goto fail;
	}
	if (cJSON_AddNumberToObject(screen, "randr_order", monitor->number)
		== NULL
	    || !cJSON_AddItemToObject(screens, monitor->si->name, screen)) {
		goto fail;
	}
	return true;

fail:
	cJSON_Delete(screen);
	return false;
}

cJSON *
fvwm_status_json_create(const char *current_screen, const char *desktop_mode,
    const monitor_topology_snapshot *topology, struct monitors *legacy_monitors,
    const FvwmWindow *focused_window, fvwm_status_desk_urgent_fn desk_is_urgent,
    fvwm_status_desk_count_fn desk_client_count)
{
	cJSON	       *message = NULL;
	cJSON	       *screens;
	struct monitor *monitor;

	if (current_screen == NULL || desktop_mode == NULL || topology == NULL
	    || legacy_monitors == NULL || desk_is_urgent == NULL
	    || desk_client_count == NULL) {
		return NULL;
	}
	message = cJSON_CreateObject();
	if (message == NULL
	    || cJSON_AddNumberToObject(message, "version", 4) == NULL
	    || cJSON_AddStringToObject(
		   message, "current_screen", current_screen)
		   == NULL
	    || cJSON_AddStringToObject(message, "desktop_mode", desktop_mode)
		   == NULL
	    || !monitor_topology_json_add(message, "topology", topology)) {
		goto fail;
	}
	screens = cJSON_AddObjectToObject(message, "screens");
	if (screens == NULL) {
		goto fail;
	}
	RB_FOREACH(monitor, monitors, legacy_monitors)
	{
		if (!status_json_add_screen(screens, monitor, focused_window,
			desk_is_urgent, desk_client_count)) {
			goto fail;
		}
	}
	return message;

fail:
	cJSON_Delete(message);
	return NULL;
}
