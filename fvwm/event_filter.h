#ifndef FVWM_EVENT_FILTER_H
#define FVWM_EVENT_FILTER_H

#include "libs/fvwm_x11.h"

typedef struct
{
	Window w;
	Atom atom;
	int event_type;
	int stop_at_event_type;
	char do_stop_at_event_type;
} flush_property_notify_args;

#endif /* FVWM_EVENT_FILTER_H */
