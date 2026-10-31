#ifndef FVWM_TRAY_CONTAINER_H
#define FVWM_TRAY_CONTAINER_H

#include "libs/tray_container_protocol.h"
#include <X11/Xlib.h>

/* Host state belongs to one Tray process. No pointers to foreign allocation
 * records survive a turn; the offer window and actual parent bind lifetime. */
typedef struct
{
	Atom capability, slot_property, offer_property, attach, acknowledge;
	Atom detach, focus, return_focus, focus_request, focus_ack;
	Window slot, offer, focus_scope;
	uint32_t focus_id;
	uint64_t focus_generation;
	double focus_deadline;
	int focus_direction;
	int width, height, root_x, root_y;
	bool capable, attached, focus_pending;
} tray_container;

typedef struct tray_ui tray_ui;
void tray_container_init(tray_ui *ui);
bool tray_container_event(tray_ui *ui, XEvent *event);
bool tray_container_geometry(tray_ui *ui);
void tray_container_detach(tray_ui *ui);
void tray_container_return(tray_ui *ui, int direction);
void tray_container_publish(tray_ui *ui);
bool tray_container_focus_request(tray_ui *ui, uint32_t id,
	int direction, Time timestamp);
void tray_container_service(tray_ui *ui, double now);
int tray_container_timeout(const tray_ui *ui, double now);

#endif
