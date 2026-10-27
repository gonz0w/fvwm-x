#ifndef FVWM_FORM_WORKAREA_H
#define FVWM_FORM_WORKAREA_H

#include "libs/module_monitor.h"
#include "libs/module_ui_scale.h"

/* Legacy reservations have no epoch. These locally associated samples are
 * discarded whenever a new MonitorUI publication replaces their topology. */
typedef struct
{
	uint64_t epoch;
	bool valid[MODULE_UI_SCALE_MAX];
	module_monitor_area areas[MODULE_UI_SCALE_MAX];
} form_workarea_state;

typedef enum
{
	FORM_WORKAREA_IGNORED,
	FORM_WORKAREA_ADMITTED,
	FORM_WORKAREA_REFUSED
} form_workarea_status;

/* Inputs borrow the validated published MonitorUI snapshot. No storage is
 * allocated, and no legacy sample certifies atomic freshness of that epoch. */
void form_workarea_sync(form_workarea_state *state,
	const module_ui_scale_snapshot *snapshot);
form_workarea_status form_workarea_receive(form_workarea_state *state,
	const module_ui_scale_snapshot *snapshot, const char *text);
/* Return physical bounds if no checked sample matches the published row. */
bool form_workarea_bounds(const form_workarea_state *state,
	const module_ui_scale_snapshot *snapshot, const module_ui_scale_row *row,
	module_monitor_area *area);

#endif
