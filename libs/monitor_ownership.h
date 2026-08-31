#ifndef FVWM_MONITOR_OWNERSHIP_H
#define FVWM_MONITOR_OWNERSHIP_H

#include "libs/fvwmrect.h"
#include "libs/monitor_topology.h"

#define MONITOR_OWNERSHIP_HYSTERESIS_PX 8

typedef enum
{
	MONITOR_OWNERSHIP_OK,
	MONITOR_OWNERSHIP_OVERFLOW,
	MONITOR_OWNERSHIP_INVALID_ARGUMENT
} monitor_ownership_status;

typedef enum
{
	MONITOR_OWNERSHIP_REASON_CONTAINMENT,
	MONITOR_OWNERSHIP_REASON_OVERLAP,
	MONITOR_OWNERSHIP_REASON_INCUMBENT,
	MONITOR_OWNERSHIP_REASON_GAP,
	MONITOR_OWNERSHIP_REASON_FALLBACK
} monitor_ownership_reason;

typedef struct
{
	Atom			 name_atom;
	const char		*name;
	monitor_ownership_reason reason;
	int			 extra_overlap;
} monitor_ownership_result;

#define MONITOR_OWNERSHIP_RESULT_SAFE_INIT                                 \
	{None, NULL, MONITOR_OWNERSHIP_REASON_FALLBACK, 0}

monitor_ownership_status
monitor_ownership_resolve(const rectangle *window,
    const monitor_topology_monitor *monitors, int monitor_count,
    Atom incumbent_atom, int hysteresis_px, monitor_ownership_result *result);

monitor_ownership_status
monitor_ownership_work_rect(const rectangle *monitor_rect, int strut_left,
    int strut_right, int strut_top, int strut_bottom, rectangle *out);

monitor_ownership_status
monitor_ownership_work_insets(const rectangle *monitor_rect,
    const rectangle *work_rect, boundingbox *out);

monitor_ownership_status
monitor_ownership_work_rect_paged(const rectangle *work_rect, int page_x,
    int page_y, rectangle *out);

#endif
