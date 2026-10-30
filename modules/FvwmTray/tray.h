#ifndef FVWM_TRAY_PRIVATE_H
#define FVWM_TRAY_PRIVATE_H

#include "tray_model.h"
#include "libs/Module.h"
#include "libs/module_packet_reader.h"
#include "libs/module_ui_scale.h"
#include "libs/module_workarea.h"
#include "libs/flocale_font_types.h"

typedef struct
{
	Window socket;
	unsigned long serial;
	bool saved;
	unsigned int border_width;
} tray_site;

typedef struct
{
	int colorset;
	unsigned int padding, gap, position;
	bool vertical, visible;
	char font_name[257];
} tray_options;

typedef struct
{
	Display *display;
	Window root, owner, host, proxy, previous_focus;
	Atom selection, manager, opcode, xembed, info, timestamp;
	Atom orientation_atom, visual_atom, host_atom, workarea_atom;
	ModuleArgs *module;
	int channel[2], wake[2], colorset;
	char prefix[132], font_name[257];
	GC gc;
	FlocaleFont *font;
	unsigned int quarters, padding, gap, position;
	bool vertical, visible, claimed, active, stopped, failed, dirty;
	bool config_pending, config_refresh, config_valid, release_pending;
	tray_options staged;
	unsigned int x_error_code;
	unsigned long x_error_resource;
	unsigned int refusals, focus_budget;
	module_packet_reader reader;
	module_ui_scale_receiver monitors;
	module_workarea_state workareas;
	module_monitor_area area;
	tray_model model;
	tray_layout layout;
	tray_site sites[TRAY_CLIENT_MAX];
} tray_ui;

bool tray_x11_start(tray_ui *ui);
void tray_x11_stop(tray_ui *ui);
void tray_x11_event(tray_ui *ui, XEvent *event);
void tray_x11_layout(tray_ui *ui);
void tray_x11_focus(tray_ui *ui, uint32_t id, int direction, Time timestamp);
void tray_x11_unfocus(tray_ui *ui, bool restore);
void tray_x11_hints(tray_ui *ui);
void tray_render(tray_ui *ui);
void tray_request_config(tray_ui *ui);

#endif
