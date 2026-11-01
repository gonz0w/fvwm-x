/* -*-c-*- */

#ifndef FVWM_ICCCM2_H
#define FVWM_ICCCM2_H

#include "libs/fvwm_x11.h"

extern void SetupICCCM2(Bool replace_wm);
extern void CloseICCCM2(void);
extern void icccm2_handle_selection_request(const XEvent *e);
extern Bool icccm2_handle_selection_event(const XEvent *event);
extern Bool icccm2_defer_selection_event(const XEvent *event);
extern void icccm2_selection_interaction(void);
extern Bool icccm2_handle_selection_error(const XErrorEvent *event);
extern void icccm2_service_selection(void);
/* Pure deadline selection: do not issue X work between FPending and poll. */
extern int icccm2_selection_timeout_ms(int other_ms);
extern void icccm2_handle_selection_clear(const XSelectionClearEvent *event);

#endif /* FVWM_ICCCM2_H */
