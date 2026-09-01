#ifndef FVWM_GEOMETRY_TRANSITION_H
#define FVWM_GEOMETRY_TRANSITION_H

#include "libs/fvwmrect.h"
#include "libs/icccm_size_hints.h"
#include "libs/monitor_scale.h"

#include <stdbool.h>

typedef enum
{
	GEOMETRY_TRANSITION_OK,
	GEOMETRY_TRANSITION_OVERFLOW,
	GEOMETRY_TRANSITION_INVALID_ARGUMENT,
	GEOMETRY_TRANSITION_INELIGIBLE,
	GEOMETRY_TRANSITION_NOOP
} geometry_transition_status;

typedef enum
{
	GEOMETRY_TRANSITION_NORMAL,
	GEOMETRY_TRANSITION_MAXIMIZED,
	GEOMETRY_TRANSITION_FULLSCREEN,
	GEOMETRY_TRANSITION_RESTORE
} geometry_transition_kind;

/* Always convert from the retained exact source, never a rounded result. */
geometry_transition_status
geometry_transition_plan(geometry_transition_kind kind,
    size_rect logical_client, size_rect src_client, bool eligible,
    monitor_scale_factor dest_factor, const size_borders *dest_chrome,
    const icccm_size_hints *hints, rectangle *dest_frame);

#endif
