#ifndef FVWM_MODULE_WORKAREA_H
#define FVWM_MODULE_WORKAREA_H

#include "libs/module_monitor.h"
#include "libs/module_ui_scale.h"

/* Legacy reservations have no epoch. These locally associated samples are
 * discarded whenever a new MonitorUI publication replaces their topology. */
typedef struct
{
	uint64_t epoch;
	bool valid[MODULE_UI_SCALE_MAX];
	module_monitor_area areas[MODULE_UI_SCALE_MAX];
} module_workarea_state;

typedef enum
{
	MODULE_WORKAREA_IGNORED,
	MODULE_WORKAREA_ADMITTED,
	MODULE_WORKAREA_REFUSED
} module_workarea_status;

/* Inputs borrow the validated published MonitorUI snapshot. No storage is
 * allocated, and no legacy sample certifies atomic freshness of that epoch. */
void module_workarea_sync(module_workarea_state *state,
			  const module_ui_scale_snapshot *snapshot);
module_workarea_status
module_workarea_receive(module_workarea_state *state,
			const module_ui_scale_snapshot *snapshot,
			const char *text);
/* Return physical bounds if no checked sample matches the published row. */
bool module_workarea_bounds(const module_workarea_state *state,
			    const module_ui_scale_snapshot *snapshot,
			    const module_ui_scale_row *row,
			    module_monitor_area *area);

#endif
