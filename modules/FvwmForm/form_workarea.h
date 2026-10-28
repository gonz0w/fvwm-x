#ifndef FVWM_FORM_WORKAREA_H
#define FVWM_FORM_WORKAREA_H

#include "libs/module_workarea.h"

typedef module_workarea_state form_workarea_state;
typedef module_workarea_status form_workarea_status;
#define FORM_WORKAREA_IGNORED MODULE_WORKAREA_IGNORED
#define FORM_WORKAREA_ADMITTED MODULE_WORKAREA_ADMITTED
#define FORM_WORKAREA_REFUSED MODULE_WORKAREA_REFUSED

void form_workarea_sync(form_workarea_state *state,
	const module_ui_scale_snapshot *snapshot);
form_workarea_status form_workarea_receive(form_workarea_state *state,
	const module_ui_scale_snapshot *snapshot, const char *text);
bool form_workarea_bounds(const form_workarea_state *state,
	const module_ui_scale_snapshot *snapshot, const module_ui_scale_row *row,
	module_monitor_area *area);

#endif
