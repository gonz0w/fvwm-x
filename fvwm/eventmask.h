/* -*-c-*- */

#ifndef FVWM_EVENTMASK_H
#define FVWM_EVENTMASK_H

#include "libs/fvwm_x11.h"
#include "libs/startup_group_policy.h"

/* Only referenced startup leaders have a retained mask record. Ordinary
 * windows keep the existing direct selection path. */
struct eventmask_scope {
	Window window;
	long base_mask;
	uint64_t serial;
	uint32_t suppressed;
	Bool tracked;
	Bool counted;
};

void eventmask_initialize(void);
struct startup_group_state *eventmask_startup_groups(void);
long eventmask_base(Window window, long server_mask);
void eventmask_select(Window window, long base_mask);
void eventmask_change_attributes(Window window, unsigned long value_mask,
	const XSetWindowAttributes *attributes);
void eventmask_suppress_begin(Window window, long server_mask, long suppressed,
	struct eventmask_scope *scope);
void eventmask_suppress_end(struct eventmask_scope *scope);
Bool eventmask_outer_ready(void);

/* ---------------------------- global definitions ------------------------- */

#define XEVMASK_FRAMEW                                                     \
	(SubstructureRedirectMask | VisibilityChangeMask | EnterWindowMask \
	    | LeaveWindowMask)
#define XEVMASK_FRAMEW_CAPTURE (XEVMASK_FRAMEW | StructureNotifyMask)
#define XEVMASK_TITLEW                                             \
	(ButtonPressMask | ButtonReleaseMask | OwnerGrabButtonMask \
	    | /*ButtonMotionMask | PointerMotionMask | */          \
	    EnterWindowMask | LeaveWindowMask)
#define XEVMASK_BUTTONW XEVMASK_TITLEW
#define XEVMASK_PARENTW (SubstructureRedirectMask)
#define XEVMASK_BORDERW                                        \
	(ButtonPressMask | ButtonReleaseMask | EnterWindowMask \
	    | LeaveWindowMask)
#define XEVMASK_CLIENTW                                             \
	(StructureNotifyMask | PropertyChangeMask | EnterWindowMask \
	    | LeaveWindowMask | ColormapChangeMask | FocusChangeMask)
#define XEVMASK_ICONW                                                        \
	(ButtonPressMask | ButtonReleaseMask | VisibilityChangeMask          \
	    | ExposureMask | KeyPressMask | KeyReleaseMask | EnterWindowMask \
	    | LeaveWindowMask | FocusChangeMask)
#define XEVMASK_ICONPW XEVMASK_ICONW
#define XEVMASK_MENU                                                         \
	(ButtonPressMask | ButtonReleaseMask | ExposureMask | KeyReleaseMask \
	    | KeyPressMask | VisibilityChangeMask | ButtonMotionMask         \
	    | PointerMotionMask)
#define XEVMASK_TEAR_OFF_MENU (XEVMASK_MENU | LeaveWindowMask | EnterWindowMask)
#define XEVMASK_MENUW (ExposureMask | KeyPressMask | KeyReleaseMask)
#define XEVMASK_TEAR_OFF_MENUW                             \
	(XEVMASK_MENUW | EnterWindowMask | LeaveWindowMask \
	    | StructureNotifyMask)
#define XEVMASK_TEAR_OFF_SUBMENUW (XEVMASK_MENUW | LeaveWindowMask)
#define XEVMASK_PANFW                                                        \
	(ButtonPressMask | ButtonReleaseMask | KeyReleaseMask | KeyPressMask \
	    | EnterWindowMask | LeaveWindowMask | VisibilityChangeMask)
#define XEVMASK_NOFOCUSW (KeyPressMask | KeyReleaseMask | FocusChangeMask)
#define XEVMASK_MENUNFW (KeyPressMask | KeyReleaseMask | FocusChangeMask)
#define XEVMASK_ORW (FocusChangeMask)
#define XEVMASK_ROOTW                                                  \
	(LeaveWindowMask | EnterWindowMask | PropertyChangeMask        \
	    | SubstructureRedirectMask | KeyPressMask | KeyReleaseMask \
	    | StructureNotifyMask | SubstructureNotifyMask             \
	    | ColormapChangeMask | ButtonPressMask | ButtonReleaseMask)
#define XEVMASK_RESIZE                                      \
	(ButtonPressMask | ButtonReleaseMask | KeyPressMask \
	    | PointerMotionMask | ButtonMotionMask | ExposureMask)
#define XEVMASK_RESIZE_OPAQUE (XEVMASK_RESIZE | PropertyChangeMask)

#endif /* FVWM_EVENTMASK_H */
