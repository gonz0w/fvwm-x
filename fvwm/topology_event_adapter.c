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

#include "topology_event_adapter.h"

#include <X11/extensions/randr.h>

fvwm_topology_event_class
fvwm_topology_event_classify(const fvwm_topology_event_input *input)
{
	if (input == NULL || input->event_window != input->root_window) {
		return FVWM_TOPOLOGY_EVENT_REJECTED;
	}
	if (input->event_type == ConfigureNotify) {
		return FVWM_TOPOLOGY_EVENT_ROOT_CONFIGURE;
	}
	if (input->event_type
	    == input->randr_event_base + RRScreenChangeNotify) {
		return FVWM_TOPOLOGY_EVENT_SCREEN_CHANGE;
	}
	if (input->event_type != input->randr_event_base + RRNotify) {
		return FVWM_TOPOLOGY_EVENT_REJECTED;
	}

	switch (input->randr_subtype) {
	case RRNotify_CrtcChange:
	case RRNotify_OutputChange:
	case RRNotify_OutputProperty:
	case RRNotify_ResourceChange:
		return FVWM_TOPOLOGY_EVENT_RANDR_NOTIFY;
	default:
		return FVWM_TOPOLOGY_EVENT_REJECTED;
	}
}

fvwm_topology_event_class
fvwm_topology_event_dispatch(const fvwm_topology_event_input *input,
    XEvent *event, const fvwm_topology_event_callbacks *callbacks,
    void *context)
{
	fvwm_topology_event_class event_class =
	    fvwm_topology_event_classify(input);

	if (event_class == FVWM_TOPOLOGY_EVENT_REJECTED || callbacks == NULL) {
		return event_class;
	}
	if (event_class == FVWM_TOPOLOGY_EVENT_SCREEN_CHANGE
	    && callbacks->update_configuration != NULL) {
		callbacks->update_configuration(context, event);
	}
	if (callbacks->mark_dirty != NULL) {
		callbacks->mark_dirty(context);
	}
	return event_class;
}

int
fvwm_topology_timeout_min(int scheduler_timeout_ms, int topology_timeout_ms)
{
	if (scheduler_timeout_ms < 0) {
		return topology_timeout_ms;
	}
	if (topology_timeout_ms < 0) {
		return scheduler_timeout_ms;
	}
	return scheduler_timeout_ms < topology_timeout_ms ? scheduler_timeout_ms
							  : topology_timeout_ms;
}

bool
fvwm_topology_refresh_requests_downstream(
    fscreen_topology_refresh_result result)
{
	return result == FSCREEN_TOPOLOGY_REFRESH_CHANGED;
}
