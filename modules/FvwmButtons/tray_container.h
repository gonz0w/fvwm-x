#ifndef FVWM_BUTTONS_TRAY_CONTAINER_H
#define FVWM_BUTTONS_TRAY_CONTAINER_H

#include "FvwmButtons.h"

Bool buttons_tray_claim(button_info *button);
Bool buttons_tray_requested(void);
void buttons_tray_forget(button_info *button);
void buttons_tray_layout(button_info *button);
void buttons_tray_service(void);
int buttons_tray_timeout(void);
Bool buttons_tray_event(XEvent *event);
Bool buttons_tray_normalize_focus(XEvent *event);
Bool buttons_tray_activate(button_info *button, int direction);
void buttons_tray_cleanup(void);
void buttons_tray_focus_change(Window window, Window frame);
void buttons_tray_destroyed(Window window);
void buttons_tray_leave(void);
Window buttons_tray_external_focus(void);
unsigned int buttons_tray_leave_requests(void);

#endif
