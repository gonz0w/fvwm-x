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
#include <errno.h>
#include <stdint.h>

#include "libs/fvwmlib.h"
#include "libs/ColorUtils.h"
#include "libs/Graphics.h"
#include "libs/FEvent.h"
#include "libs/fvwmsignal.h"
#include "libs/log.h"
#include "../script_delay.h"
#include "Tools.h"

/*
 * Functions for MiniScroll
 */
void InitMiniScroll(struct XObj *xobj)
{
  unsigned long mask;
  XSetWindowAttributes Attr;
  int i;

  /* Save colors */
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

  /* The widget size is fixed */
  xobj->width = 19;
  xobj->height = 34;
  xobj->win=XCreateWindow(dpy, *xobj->ParentWin,
			  xobj->x, xobj->y, xobj->width, xobj->height, 0,
			  CopyFromParent, InputOutput, CopyFromParent,
			  mask, &Attr);
  xobj->gc=fvwmlib_XCreateGC(dpy, xobj->win, 0, NULL);
  if (xobj->colorset >= 0)
    SetWindowBackground(dpy, xobj->win, xobj->width, xobj->height,
			GetColorset(xobj->colorset), Pdepth,
			xobj->gc, True);
  XSetForeground(dpy, xobj->gc, xobj->TabColor[fore]);
  XSetLineAttributes(dpy, xobj->gc, 1, LineSolid, CapRound, JoinMiter);
  if (xobj->value2>xobj->value3)
  {
    i = xobj->value2;
    xobj->value2 = xobj->value3;
    xobj->value3 = i;
  }
  if ((xobj->value < xobj->value2) || (xobj->value > xobj->value3))
    xobj->value = xobj->value2;
  XSelectInput(dpy, xobj->win, ExposureMask);
}

void DrawMiniScroll(struct XObj *xobj, XEvent *evp)
{

  DrawReliefRect(-1, -1, xobj->width+2, xobj->height+2, xobj, hili, shad);

  DrawArrowN(xobj, 3, 3, 0);  /* top arrow    */
  DrawArrowS(xobj, 3, 18, 0); /* bottom arrow */
}


void EvtMouseMiniScroll(struct XObj *xobj, XButtonEvent *EvtButton)
{
  int x1,y1,x2,y2;
  Window Win1,Win2;
  unsigned int modif;
  int64_t local_x, local_y;
  int Pos = 0;
  int count = 0;

  do
  {
    if (count == 1)
    {
      /* emulate auto repeat delay */
      if (!script_widget_wait_release(xobj, 16))
      {
        break;
      }
      count = 2;
      continue;
    }
    FQueryPointer(dpy, *xobj->ParentWin, &Win1, &Win2,
		  &x1, &y1, &x2, &y2, &modif);
    /* Find current option */
    local_y = (int64_t)y2 - xobj->y;
    local_x = (int64_t)x2 - xobj->x;
    if ((local_x > 0) && (local_x < xobj->width) &&
        (local_y > 0) && (local_y < xobj->height/2))
    {
      /* top arrow */
      if (Pos == 1)
      {
	if (!script_widget_wait_release(xobj, 8))
	{
	  break;
	}
      }
      else
      {
	DrawArrowN(xobj, 3, 3, 1);
	Pos = 1;
      }
      xobj->value = script_widget_step(xobj->value, xobj->value2,
        xobj->value3, true);
      SendMsg(xobj,SingleClic);
    }
    else if ( (local_x > 0) && (local_x < xobj->width) &&
              (local_y > xobj->height/2) && (local_y < xobj->height))
    {
      if (Pos == -1)
      {
	if (!script_widget_wait_release(xobj, 8))
	{
	  break;
	}
      }
      else
      {
	DrawArrowS(xobj, 3, 18, 1);
	Pos = -1;
      }
      xobj->value = script_widget_step(xobj->value, xobj->value2,
        xobj->value3, false);
      SendMsg(xobj,SingleClic);
    }
    else if (Pos!=0)
    {
      Pos = 0;
      DrawArrowN(xobj, 3, 3, 0);
      DrawArrowS(xobj, 3, 18, 0);
    }
    /* Only the first two iterations need a distinct repeat-delay state. */
    if (count == 0)
    {
      count = 1;
    }
  }
  while (EvtButton != NULL && script_widget_wait_release(xobj, 1));
  DrawArrowN(xobj, 3, 3, 0);
  DrawArrowS(xobj, 3, 18, 0);
}

void EvtKeyMiniScroll(struct XObj *xobj, XKeyEvent *EvtKey)
{
  KeySym ks;
  unsigned char buf[10];

  XLookupString(EvtKey, (char *)buf, sizeof(buf), &ks, NULL);
  if (ks == XK_Return) {
    EvtMouseMiniScroll(xobj, NULL);
  }
  else if (ks == XK_Up) {
    xobj->value = script_widget_step(xobj->value, xobj->value2,
        xobj->value3, true);
    SendMsg(xobj,SingleClic);
  }
  else if (ks == XK_Down) {
    xobj->value = script_widget_step(xobj->value, xobj->value2,
        xobj->value3, false);
    SendMsg(xobj,SingleClic);
  }
}
