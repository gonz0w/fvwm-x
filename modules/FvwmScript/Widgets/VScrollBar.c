/* -*-c-*- */
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

#include "config.h"

#include "libs/ColorUtils.h"
#include "libs/FEvent.h"
#include "libs/fvwmlib.h"
#include "libs/Graphics.h"
#include "libs/log.h"
#include "Tools.h"

/*
 * Function for VScrollBar
 */
void DrawThumbV(struct XObj *xobj, XEvent *evp)
{
	int x,y,w,h;
	XSegment segm;
	char str[20];

	x = xobj->width/2 - 10;
	y = 2 + script_widget_position(xobj->value, xobj->value2,
    xobj->value3, xobj->height, 36, true);
	w = 20;
	h = 32;
	DrawReliefRect(x, y, w, h, xobj, hili, shad);
	segm.x1 = x + 3;
	segm.y1 = y + 15;
	segm.x2 = x + w - 3;
	segm.y2 = y + 15;
	XSetForeground(dpy, xobj->gc, xobj->TabColor[shad]);
	XDrawSegments(dpy, xobj->win, xobj->gc, &segm, 1);
	segm.x1 = x + 3;
	segm.y1 = y + 16;
	segm.x2 = x + w - 3;
	segm.y2 = y + 16;
	XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
	XDrawSegments(dpy, xobj->win, xobj->gc, &segm, 1);
	XSetForeground(dpy, xobj->gc, xobj->TabColor[fore]);

	snprintf(str, sizeof(str), "%d", xobj->value);
	x = x-FlocaleTextWidth(xobj->Ffont, str, strlen(str))-6;
	y = y + 13 + xobj->Ffont->ascent/2;
	MyDrawString(dpy, xobj, xobj->win, x, y, str, fore, hili, back,
		     !xobj->flags[1], NULL, evp);
}

void HideThumbV(struct XObj *xobj)
{
	int x,y;
	char str[20];

	x = xobj->width/2 - 10;
	y = 2 + script_widget_position(xobj->value, xobj->value2,
    xobj->value3, xobj->height, 36, true);
	XClearArea(dpy, xobj->win, x, y, 20, 32, False);
	snprintf(str, sizeof(str), "%d", xobj->value);
	XClearArea(
		dpy, xobj->win, 0, 0,xobj->width/2 - 14,xobj->height, False);
}

void InitVScrollBar(struct XObj *xobj)
{
	unsigned long mask;
	XSetWindowAttributes Attr;

	/* Save colors and font */
	if (xobj->colorset >= 0) {
		xobj->TabColor[fore] = GetColorset(xobj->colorset)->fg;
		xobj->TabColor[back] = GetColorset(xobj->colorset)->bg;
		xobj->TabColor[hili] = GetColorset(xobj->colorset)->hilite;
		xobj->TabColor[shad] = GetColorset(xobj->colorset)->shadow;
	} else {
		xobj->TabColor[fore] = GetColor(xobj->forecolor);
		xobj->TabColor[back] = GetColor(xobj->backcolor);
		xobj->TabColor[hili] = GetColor(xobj->hilicolor);
		xobj->TabColor[shad] = GetColor(xobj->shadcolor);
	}

	mask = 0;
	Attr.background_pixel = xobj->TabColor[back];
	mask |= CWBackPixel;
	if (!x11base->cursor) {
		Attr.cursor = XCreateFontCursor(dpy,XC_hand2);
		mask |= CWCursor;  /* Window cursor */
	}

	xobj->win = XCreateWindow(
		dpy, *xobj->ParentWin, xobj->x, xobj->y, xobj->width,
		xobj->height, 0, CopyFromParent, InputOutput, CopyFromParent,
		mask, &Attr);
	xobj->gc = fvwmlib_XCreateGC(dpy, xobj->win, 0, NULL);
	XSetForeground(dpy, xobj->gc, xobj->TabColor[fore]);


	if (xobj->Ffont->font != NULL)
		XSetFont(dpy, xobj->gc, xobj->Ffont->font->fid);

	XSetLineAttributes(dpy, xobj->gc, 1, LineSolid, CapRound, JoinMiter);

	xobj->value3 = script_widget_initial_maximum(xobj->value2,
    xobj->value3, 10, true);
	if (!((xobj->value >= xobj->value2) && (xobj->value <= xobj->value3)))
		xobj->value = xobj->value2;

	if (xobj->colorset >= 0)
		SetWindowBackground(dpy, xobj->win, xobj->width, xobj->height,
				    GetColorset(xobj->colorset), Pdepth,
				    xobj->gc, True);
	XSelectInput(dpy, xobj->win, ExposureMask);
}

void DestroyVScrollBar(struct XObj *xobj)
{
	FlocaleUnloadFont(dpy,xobj->Ffont);
	XFreeGC(dpy,xobj->gc);
	XDestroyWindow(dpy,xobj->win);
}

void DrawVScrollBar(struct XObj *xobj, XEvent *evp)
{
	int x,y,w,h;
	char str[20];

	/* Compute scrollbar size */
	x = xobj->width/2 - 12;
	y = 0;
	w = 24;
	h = xobj->height;
	DrawThumbV(xobj, evp);
	DrawReliefRect(x, y, w, h, xobj, shad, hili);
	/* Write values */
	x = x + 26;
	y = xobj->Ffont->ascent + 2;
	snprintf(str, sizeof(str), "%d", xobj->value3);
	MyDrawString(dpy, xobj, xobj->win, x, y, str, fore, hili, back,
		     !xobj->flags[1], NULL, evp);
	snprintf(str, sizeof(str), "%d", xobj->value2);
	y = h - xobj->Ffont->descent - 2;
	MyDrawString(dpy, xobj, xobj->win, x, y, str, fore, hili, back,
		     !xobj->flags[1], NULL, evp);
}

void EvtMouseVScrollBar(struct XObj *xobj, XButtonEvent *EvtButton)
{
	XEvent event;
	int oldy = 0;
	int oldvalue = -1;
	int newvalue;
	int x1,y1,x2,y2;
	Window Win1,Win2;
	unsigned int modif;

  if (xobj->height <= 36 || xobj->value3 <= xobj->value2)
    return;

	do
	{
		/* We follow mouse movements */
		FQueryPointer(dpy, *xobj->ParentWin, &Win1, &Win2,
			      &x1, &y1, &x2, &y2, &modif);
		y2 = script_widget_pointer_position(y2, xobj->y,
      xobj->height);
		if (oldy != y2)
		{
			oldy = y2;
			/* compute xobj->value */
			newvalue = script_widget_pointer_value(y2, xobj->height,
      xobj->value2, xobj->value3, true);
			if (newvalue!=oldvalue)
			{
				HideThumbV(xobj);
				xobj->value = newvalue;
				DrawThumbV(xobj, NULL);
				oldvalue = newvalue;
				SendMsg(xobj,SingleClic);
				XSync(dpy,0);
			}
		}
    /* Return selects once; queued release precedes shared readiness. */
    if (EvtButton == NULL || FCheckTypedEvent(dpy, ButtonRelease, &event))
      break;
    if (!script_widget_wait_release(xobj, 1))
      break;
  }
  while (True);
}

void EvtKeyVScrollBar(struct XObj *xobj, XKeyEvent *EvtKey)
{
	KeySym ks;
	unsigned char buf[10];

	XLookupString(EvtKey, (char *)buf, sizeof(buf), &ks, NULL);
	if (ks == XK_Down && xobj->value > 0) {
		HideThumbV(xobj);
		xobj->value--;
		DrawThumbV(xobj, NULL);
		SendMsg(xobj,SingleClic);
	}
	else if (ks == XK_Up &&
		 xobj->value < xobj->value3) {
		HideThumbV(xobj);
		xobj->value++;
		DrawThumbV(xobj, NULL);
		SendMsg(xobj,SingleClic);
	}
	else if (ks == XK_Return) {
		EvtMouseVScrollBar(xobj, NULL);
	}
}
