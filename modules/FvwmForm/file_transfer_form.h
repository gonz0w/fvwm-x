#ifndef FVWM_FORM_FILE_TRANSFER_H
#define FVWM_FORM_FILE_TRANSFER_H

#include <stdbool.h>
#include <event2/event.h>
#include <X11/Xlib.h>
#include "FvwmForm.h"

/* Literal metadata only. No selection targets, URI data or names cross here. */
void form_transfer_configure(char *text);
bool form_transfer_requested(void);
void form_transfer_start(struct event_base *base);
void form_transfer_cleanup(void);
int form_transfer_fd(void);
short form_transfer_poll_events(void);
bool form_transfer_waiting(void);
void form_transfer_reset(void);
void form_transfer_generation(void);
bool form_transfer_event(XEvent *event);
bool form_transfer_xerror(const XErrorEvent *error);
void form_transfer_border(Item *item);

#endif
