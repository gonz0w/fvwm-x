#ifndef FVWM_EWMH_PROPERTY_POLICY_H
#define FVWM_EWMH_PROPERTY_POLICY_H

#include "libs/FProperty.h"
#include <string.h>

#define FVWM_EWMH_TEXT_MAX_BYTES (64U * 1024U)
#define FVWM_EWMH_LIST_MAX_BYTES (16U * 1024U)
/* Preserve room for the existing 4096x4096 icon ceiling plus smaller icons. */
#define FVWM_EWMH_ICON_MAX_BYTES (64U * 1024U * 1024U + 64U * 1024U)

static inline FPropertyLimits
fvwm_ewmh_property_limits(const char *name, int format)
{
	/* Empty text and atom lists are valid; fixed records override this. */
	FPropertyLimits limits = {.min_bytes = 0,
	    .max_bytes = format == 8 ? FVWM_EWMH_TEXT_MAX_BYTES :
	    FVWM_EWMH_LIST_MAX_BYTES, .format = format};

	if (strcmp(name, "_NET_WM_ICON") == 0)
	{
		limits.min_bytes = 8;
		limits.max_bytes = FVWM_EWMH_ICON_MAX_BYTES;
	}
	else if (strcmp(name, "_NET_WM_STRUT") == 0 ||
	    strcmp(name, "_NET_WM_ICON_GEOMETRY") == 0)
	{
		limits.min_bytes = limits.max_bytes = 16;
	}
	else if (strcmp(name, "_NET_WM_DESKTOP") == 0 ||
	    strcmp(name, "_NET_WM_PID") == 0 ||
	    strcmp(name, "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR") == 0)
	{
		limits.min_bytes = limits.max_bytes = 4;
	}
	return limits;
}

#endif
