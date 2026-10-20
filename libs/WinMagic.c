/* -*-c-*- */
/* Copyright (C) 1999  Dominik Vogt */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

/*
** WinMagic.c:
** This file supplies routines for moving and resizing windows in an animated
** fashion.
*/

#include "config.h"

#include <limits.h>
#include <stdint.h>

#include <X11/Xlib.h>
#include <stdio.h>

#include "fvwmlib.h"
#include "FEvent.h"
#include "WinMagic.h"

/* Continuously moves and resized window win from start geometry (s_?) to end
 * geometry (e_?). Waits for delay_ms milliseconds after each step except the
 * last (max. 10 seconds per step). The number of steps is determined by the
 * steps argument (min. 1 and max. 10000). If the pointer ppctMovement is NULL
 * the steps are all the same width, if it is given it is interpreted as a
 * pointer to an array of float percent values. These are used to determine the
 * distance of each step counted from the start position, i.e. 0.0 means the
 * start position itself, 50.0 is halfway between start and end position and
 * 100.0 is the end position. Values smaller than 0.0 or bigger than 100.0 are
 * allowed too. The do_flush flag determines of all requests are sent to the
 * X server immediately (True) or not (False). The use_hints detrmines if the
 * min size and resize hints are used */
void SlideWindow(
	Display *dpy, Window win,
	int s_x, int s_y, int s_w, int s_h,
	int e_x, int e_y, int e_w, int e_h,
	int steps, int delay_ms, float *ppctMovement,
	Bool do_sync, Bool use_hints)
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	int g_w = 0;
	int g_h = 0;  /* -Wall fixes :o( */
	int min_w = 1;
	int min_h = 1;
	int inc_w = 1;
	int inc_h = 1;
	int i;
	unsigned int us;
	Bool is_mapped;
	Bool keep_x1 = False;
	Bool keep_x2 = False;
	Bool keep_y1 = False;
	Bool keep_y2 = False;
	XSizeHints hints;
	long dummy;

	/* check limits */
	if (delay_ms > 10000)
	{
		/* max. 10 seconds per step */
		us = 10000000;
	}
	else if (delay_ms < 0)
	{
		us = 0;
	}
	else
	{
		us = 1000 * delay_ms;
	}

	if (steps > 10000)
	{
		/* max. 10000 steps */
		steps = 10000;
	}
	if (steps <= 0)
	{
		/* no steps, no animation */
		if (e_w == 0 || e_h == 0)
		{
			XUnmapWindow(dpy, win);
		}
		else
		{
			XMoveResizeWindow(dpy, win, e_x, e_y, e_w, e_h);
			XMapWindow(dpy, win);
			XMapSubwindows(dpy, win);
		}
		if (do_sync)
			XSync(dpy, 0);
		return;
	}

	is_mapped = False;

	/* Get the mini (re)size hints and do some check consistency */
	if (use_hints && FGetWMNormalHints(dpy, win, &hints, &dummy))
	{
		if (hints.flags & PMinSize)
		{
			if (hints.min_width >= 1 && hints.min_width <=
			    max(e_w,s_w))
				min_w = hints.min_width;
			if (hints.min_height >= 1 && hints.min_height <=
			    max(e_h,s_h))
				min_h = hints.min_height;
		}
		if (hints.flags & PResizeInc)
		{
			if (hints.width_inc >= 1 && hints.width_inc <=
			    max(e_w,s_w))
				inc_w = hints.width_inc;
			if (hints.height_inc >= 1 && hints.width_inc <=
			    max(e_h,s_h))
				inc_h = hints.height_inc;
		}
	}

	if (s_x == e_x)
		keep_x1 = True;
	if (s_y == e_y)
		keep_y1 = True;
	if (s_x + s_w == e_x + e_w)
		keep_x2 = True;
	if (s_y + s_h == e_y + e_h)
		keep_y2 = True;

	/* animate the window */
	for (i = 0; i <= steps; i++)
	{
		if (i == steps)
		{
			x = e_x;
			y = e_y;
		}
		else
		{
			float f;

			if (ppctMovement == NULL)
			{
				f = (float)i / (float)steps;
			}
			else
			{
				f = ppctMovement[i] / (float)steps;
			}
			x = (int)((float)s_x + (float)(e_x - s_x) * f);
			y = (int)((float)s_y + (float)(e_y - s_y) * f);
		}
		w = s_w + (int)(e_w - s_w) * i / steps;
		h = s_h + (int)(e_h - s_h) * i / steps;

		/* take the resize inc in account */
		g_w = w - ((w - min_w) % inc_w);
		x += w-g_w;
		g_h = h - ((h - min_h) % inc_h);
		y += h-g_h;

		/* prevent annoying flickering */
		if (keep_x1)
			x = s_x;
		if (keep_y1)
			y = s_y;
		if (keep_x2)
			g_w = s_x + s_w - x;
		if (keep_y2)
			g_h = s_y + s_h - y;

		if (g_w < min_w || g_h < min_h)
		{
			/* don't show zero width/height windows */
			if (is_mapped)
			{
				XUnmapWindow(dpy, win);
				is_mapped = False;
			}
		}
		else
		{
			XMoveResizeWindow(dpy, win, x, y, g_w, g_h);
			if (!is_mapped)
			{
				XMapWindow(dpy, win);
				XMapSubwindows(dpy, win);
				is_mapped = True;
			}
		}
		/* make sure everything is updated */
		if (do_sync)
			XSync(dpy, 0);
		if (us && i < steps && is_mapped)
		{
			/* don't sleep after the last step */
			usleep(us);
		}
	} /* for */

	/* if hints and asked size do not agree try to respect the caller */
	if (e_w > 0 && e_h > 0 && (!is_mapped || g_w != w || g_h != w))
	{
		XMoveResizeWindow(dpy, win, x, y, w, h);
		if (!is_mapped)
		{
			XMapWindow(dpy, win);
			XMapSubwindows(dpy, win);
		}
		if (do_sync)
			XSync(dpy, 0);
	}
	return;
}

/* Each reply is owned only for this query. The root must remain stable;
 * concurrent reparenting can otherwise make an ancestor walk revisit windows. */
Bool FQueryWindowParent(
	Display *dpy, Window window, FWindowTreeWalk *walk, Window *parent)
{
	Window root = None;
	Window next = None;
	Window *children = NULL;
	unsigned int count = 0;
	Status status;

	*parent = None;
	if (window == None || walk->queries >= F_WINDOW_TREE_MAX_QUERIES)
		return False;
	walk->queries++;
	status = XQueryTree(dpy, window, &root, &next, &children, &count);
	if (children != NULL)
		XFree(children);
	if (!status || root == None || next == None || next == window ||
	    (walk->root != None && walk->root != root))
		return False;
	walk->root = root;
	*parent = next;
	return True;
}

/* Returns None for a top-level input, failed query or exhausted walk. */
Window GetTopAncestorWindow(Display *dpy, Window child)
{
	FWindowTreeWalk walk = {0};
	Window window = child;
	Window parent;

	while (FQueryWindowParent(dpy, window, &walk, &parent))
	{
		if (parent == walk.root)
			return window == child ? None : window;
		window = parent;
	}
	return None;
}

/* Publish offsets only after a complete walk. A failed/racing hierarchy must
 * not leave the caller with a partial translation or overflowing coordinates. */
Window FGetFrameWindow(Display *dpy, Window window, int *off_x, int *off_y)
{
	FWindowTreeWalk walk = {0};
	Window parent;
	int64_t x = *off_x;
	int64_t y = *off_y;
	XWindowAttributes attributes;

	while (FQueryWindowParent(dpy, window, &walk, &parent))
	{
		if (parent == walk.root)
		{
			*off_x = (int)x;
			*off_y = (int)y;
			return window;
		}
		if (!XGetWindowAttributes(dpy, window, &attributes))
			return None;
		x += (int64_t)attributes.x + attributes.border_width;
		y += (int64_t)attributes.y + attributes.border_width;
		if (x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX)
			return None;
		window = parent;
	}
	return None;
}

/* Given a parent window this function returns a list of children of the
 * parent window that have the same size, depth, visual and colormap as the
 * parent window and that have position +0+0 within the parent. If the 'depth'
 * argument is non-zero it must match the depth of the window. The 'visualid'
 * and 'colormap' arguments work just the same. The number of matching
 * children is returned. The list of children is returned in *children. If this
 * list is non-NULL, it must be free'd with XFree. If an error occurs or the
 * parent window does not match the required depth, colormap or visualid, the
 * function returns -1 and NULL in *children. */
int GetEqualSizeChildren(
	Display *dpy, Window parent, int depth, VisualID visualid,
	Colormap colormap, Window **ret_children)
{
	XWindowAttributes pxwa;
	XWindowAttributes cxwa;
	Window JunkW;
	Window *children = NULL;
	unsigned int nchildren;
	int i;
	int j;

	*ret_children = NULL;
	if (!XGetWindowAttributes(dpy, parent, &pxwa))
		return -1;
	if (depth && pxwa.depth != depth)
		return -1;
	if (visualid && XVisualIDFromVisual(pxwa.visual) != visualid)
		return -1;
	if (colormap && pxwa.colormap != colormap)
		return -1;
	/* Validate the parent before acquiring the owned child list. */
	if (!XQueryTree(dpy, parent, &JunkW, &JunkW, &children, &nchildren))
		return -1;

	for (i = 0, j = 0; i < nchildren; i++)
	{
		if (XGetWindowAttributes(dpy, children[i], &cxwa) &&
		    cxwa.x == 0 &&
		    cxwa.y == 0 &&
		    cxwa.width == pxwa.width &&
		    cxwa.height == pxwa.height &&
		    (!depth || cxwa.depth == depth) &&
		    (
			    !visualid ||
			    XVisualIDFromVisual(cxwa.visual) == visualid) &&
		    cxwa.class == InputOutput &&
		    (!colormap || cxwa.colormap == colormap))
		{
			children[j++] = children[i];
		}
	} /* for */

	if (j == 0)
	{
		if (children)
		{
			XFree(children);
			children = NULL;
		}
	}
	*ret_children = children;

	return j;
}
