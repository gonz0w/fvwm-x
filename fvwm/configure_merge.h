#ifndef FVWM_CONFIGURE_MERGE_H
#define FVWM_CONFIGURE_MERGE_H

#include <X11/Xlib.h>
#include <X11/Xatom.h>

/* Transfer only fields omitted by the newer request. Stack/sibling requests
 * remain in their original events and retain their dispatch order. */
static inline void fvwm_merge_configure_fields(XConfigureRequestEvent *older,
    XConfigureRequestEvent *newer)
{
	unsigned long carry = older->value_mask & ~newer->value_mask;
	const unsigned long geometry = CWX | CWY | CWWidth | CWHeight |
	    CWBorderWidth;

	if (carry & CWX)
	{
		newer->x = older->x;
	}
	if (carry & CWY)
	{
		newer->y = older->y;
	}
	if (carry & CWWidth)
	{
		newer->width = older->width;
	}
	if (carry & CWHeight)
	{
		newer->height = older->height;
	}
	if (carry & CWBorderWidth)
	{
		newer->border_width = older->border_width;
	}
	newer->value_mask |= older->value_mask & geometry;
	older->value_mask &= ~geometry;
}

static inline Bool fvwm_configure_merge_hint_barrier(const XEvent *event,
    Window window)
{
	return event->type == PropertyNotify &&
	    event->xproperty.window == window &&
	    event->xproperty.atom == XA_WM_NORMAL_HINTS;
}

#endif
