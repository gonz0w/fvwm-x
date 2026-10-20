#ifndef FVWMLIB_WINMAGIC
#define FVWMLIB_WINMAGIC

#include "fvwm_x11.h"

void SlideWindow(
	Display *dpy, Window win,
	int s_x, int s_y, int s_w, int s_h,
	int e_x, int e_y, int e_w, int e_h,
	int steps, int delay_ms, float *ppctMovement,
	Bool do_sync, Bool use_hints);

/* Per-operation work limits, independent of the server's tree depth. */
#define F_WINDOW_TREE_MAX_QUERIES 4096U

typedef struct
{
	Window root;
	unsigned int queries;
} FWindowTreeWalk;

Bool FQueryWindowParent(
	Display *dpy, Window window, FWindowTreeWalk *walk, Window *parent);
Window FGetFrameWindow(Display *dpy, Window window, int *off_x, int *off_y);

Window GetTopAncestorWindow(Display *dpy, Window child);

int GetEqualSizeChildren(
	Display *dpy, Window parent, int depth, VisualID visualid,
	Colormap colormap, Window **ret_children);

#endif /* FVWMLIB_WINMAGIC */
