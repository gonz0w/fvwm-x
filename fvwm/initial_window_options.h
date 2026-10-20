#ifndef FVWM_INITIAL_WINDOW_OPTIONS_H
#define FVWM_INITIAL_WINDOW_OPTIONS_H

#include "fvwm.h"

/* This structure carries information about the initial window state and
 * placement.  This information is gathered at various places: the (re)capture
 * code, AddToWindow(), HandleMapRequestRaised(), ewmh_events.c and others.
 *
 * initial_state
 *   The initial window state.  By default it carries the value DontCareState.
 *   Other states can be set if
 *    - an icon is restarted with session management
 *    - the StartIconic style is set
 *    - GNOME, EWMH, foobar hints demand that the icon starts iconic
 *   The final value is calculated in HandleMapRequestRaised().
 *
 * do_override_ppos
 *   This flag is used in PlaceWindow().  If it is set, the position requested
 *   by the program is ignored unconditionally.  This is used during the
 *   initial capture.
 *
 * is_iconified_by_parent
 *   Preserves the information if the window is a transient window that was
 *   iconified along with its transientfor window.  Set when the window is
 *   recaptured and used in HandleMapRequestRaised() to set the according
 *   window state flag.  Deleted afterwards.
 *
 * is_menu
 *   Set in menus.c if the new window is a tear off
 *   menu.  Such windows get special treatment in AddWindow() and events.c.
 *
 * default_icon_x/y
 *   The icon position that was requested by the application in the icon
 *   position hint.  May be overridden by a style (0/0 then).  Set in
 *   HandleMapRequestRaised() and used in the icon placement code.
 *
 * initial_icon_x/y
 *   The icon position that is forced during a restart with SM.  If set it
 *   overrides all other methods of icon placement.  Set by session.c and used
 *   in the icon placement code.
 *
 * use_initial_icon_xy
 *   If set, the initial_icon_x/y values are used.  Other wise they are
 *   ignored.
 */
struct initial_window_options
{
	long initial_state;
	struct
	{
		unsigned do_override_ppos : 1;
		unsigned is_iconified_by_parent : 1;
		unsigned is_menu : 1;
		unsigned use_initial_icon_xy : 1;
	} flags;
	int initial_icon_x;
	int initial_icon_y;
	int default_icon_x;
	int default_icon_y;
};

#endif
