/* -*-c-*- */

#ifndef FVWM_TOPOLOGY_EVENT_ADAPTER_H
#define FVWM_TOPOLOGY_EVENT_ADAPTER_H

#include "libs/FScreen.h"

#include <X11/Xlib.h>
#include <stdbool.h>

typedef struct
{
	int    event_type;
	int    randr_event_base;
	int    randr_subtype;
	Window event_window;
	Window root_window;
} fvwm_topology_event_input;

typedef enum
{
	FVWM_TOPOLOGY_EVENT_REJECTED,
	FVWM_TOPOLOGY_EVENT_ROOT_CONFIGURE,
	FVWM_TOPOLOGY_EVENT_SCREEN_CHANGE,
	FVWM_TOPOLOGY_EVENT_RANDR_NOTIFY
} fvwm_topology_event_class;

typedef struct
{
	void (*update_configuration)(void *context, XEvent *event);
	void (*mark_dirty)(void *context);
} fvwm_topology_event_callbacks;

typedef struct
{
	void (*recompute_scale)(void *context);
	void (*update_ewmh)(void *context);
	void (*emit_broadcast)(void *context);
	void (*initialize_pan_frames)(void *context);
	void (*send_status)(void *context);
} fvwm_topology_downstream_callbacks;

fvwm_topology_event_class
fvwm_topology_event_classify(const fvwm_topology_event_input *input);
fvwm_topology_event_class
fvwm_topology_event_dispatch(const fvwm_topology_event_input *input,
    XEvent *event, const fvwm_topology_event_callbacks *callbacks,
    void *context);
int
fvwm_topology_timeout_min(int scheduler_timeout_ms, int topology_timeout_ms);
bool
fvwm_topology_refresh_requests_downstream(
    fscreen_topology_refresh_result result);
void
fvwm_topology_dispatch_refresh_result(fscreen_topology_refresh_result result,
    const fvwm_topology_downstream_callbacks *callbacks, void *context);

#endif
