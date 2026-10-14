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

#include "libs/fvwmlib.h"
#include "libs/ColorUtils.h"
#include "libs/Graphics.h"
#include "libs/FEvent.h"
#include "libs/log.h"
#include "Tools.h"

#define BdWidth 2               /* Border width */
#define SbWidth 15              /* ScrollBar width */

/*
 * Functions for the List
 */
void InitList(struct XObj *xobj)
{
  unsigned long mask;
  XSetWindowAttributes Attr;
  int NbVisCell,NbCell;

  /* Save font and colors */
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
  xobj->win = XCreateWindow(dpy, *xobj->ParentWin,
			  xobj->x, xobj->y, xobj->width, xobj->height, 0,
			  CopyFromParent, InputOutput, CopyFromParent,
			  mask, &Attr);

  xobj->gc = fvwmlib_XCreateGC(dpy, xobj->win, 0, NULL);
  XSetForeground(dpy, xobj->gc, xobj->TabColor[fore]);
  XSetLineAttributes(dpy, xobj->gc, 1, LineSolid, CapRound, JoinMiter);

  if (xobj->Ffont->font != NULL)
    XSetFont(dpy, xobj->gc, xobj->Ffont->font->fid);

  if (xobj->colorset >= 0)
    SetWindowBackground(dpy, xobj->win, xobj->width, xobj->height,
			GetColorset(xobj->colorset), Pdepth,
			xobj->gc, True);

  /* Computation of the first visible cell */
  NbVisCell = (script_widget_dimension(xobj->height) - 2 - 3*BdWidth) /
      script_list_row_height(xobj->Ffont->height);
  NbCell = CountOption(xobj->title);
  xobj->value2 = script_list_first(xobj->value2, NbCell, NbVisCell);
  XSelectInput(dpy, xobj->win, ExposureMask);
}

void DrawVSbList(struct XObj *xobj, int NbCell, int NbVisCell, int press)
{
  int width = script_widget_dimension(xobj->width);
  int height = script_widget_dimension(xobj->height);
  XRectangle r;
  int PosTh,SizeTh;

  if (width < 24 || height < 13)
    return;
  r.y = 2+BdWidth;
  r.x = width - (6+BdWidth) -SbWidth;
  r.height = height - r.y - 2*BdWidth;
  r.width = SbWidth + 4;
  DrawReliefRect(r.x, r.y, r.width, r.height, xobj, shad, hili);

  /* Compute the rectangle for the arrows */
  r.x = r.x + 2;
  r.y = r.y + 2;
  r.width = r.width - 4;
  r.height = r.height - 4;

  /* Draw the up arrow */
  DrawArrowN(xobj, r.x + 1, r.y + 1, press==1);
  DrawArrowS(xobj, r.x + 1, r.y + r.height - 14, press==2);

  /* Compute rectangle for thumb */
  r.y = r.y + 13;
  if (r.height <= 26)
    return;
  r.height = r.height - 26;

  /* Erase */
  if (r.height > 2)
    XClearArea(dpy, xobj->win, r.x, r.y+1, r.width, r.height-2, False);

  /* Compute in wide integers, preserving the legacy count denominator. */
  script_list_thumb(xobj->value2, NbCell, NbVisCell, r.height,
      &PosTh, &SizeTh);
  DrawReliefRect(r.x, r.y + PosTh, r.width, SizeTh, xobj, hili, shad);
}

void DrawCellule(
	struct XObj *xobj, int NbCell, int NbVisCell, int HeightCell,
	int asc, XEvent *evp)
{
  int width = script_widget_dimension(xobj->width);
  int height = script_widget_dimension(xobj->height);
  XRectangle r,inter;
  char *Title;
  int64_t i;
  int do_draw = True;
  size_t option_length;
  const char *option = script_text_option(xobj->title, xobj->value2,
      &option_length);

  if (width < 42 || height < 15 || HeightCell < 3)
    return;
  /* text clipping */
  r.x = 4 + BdWidth + 4;
  r.y = 4 + BdWidth;
  r.width =  width - 16 - SbWidth - BdWidth - 6;
  r.height = height - r.y - 4 - 2*BdWidth;

  for (i = xobj->value2; i < (int64_t)xobj->value2 + NbVisCell; i++)
  {
	  const char *current = option;
	  option_length = strcspn(option, "|");
	  option += option_length;
	  if (*option == '|')
		  option++;
	  do_draw = True;
	  if (evp)
	  {
		  if (!frect_get_intersection(
			  r.x - 2, r.y + (i - xobj->value2)*HeightCell + 2,
			  r.width + 4, HeightCell-2,
			  evp->xexpose.x, evp->xexpose.y,
			  evp->xexpose.width, evp->xexpose.height,
			  &inter))
		  {
			  do_draw = False;
		  }
	  }
	  else
	  {
		  inter.x = r.x - 2; /* r.x */
		  inter.y = r.y + (i-xobj->value2)*HeightCell + 2;
		  inter.width = r.width + 4;
		  inter.height = HeightCell-2;
	  }
	  if (!do_draw)
	  {
		  continue;
	  }
	  /* Allocate only rows that will be drawn, after the expose check. */
	  Title = fxmalloc(option_length + 1);
	  memcpy(Title, current, option_length);
	  Title[option_length] = '\0';
	  if (option_length != 0)
	  {
		  int x = GetXTextPosition(
			  xobj, r.width,
			  FlocaleTextWidth(xobj->Ffont,Title,option_length),
			  r.x, 0, -r.x);
		  int y = (i - xobj->value2) * HeightCell + asc + 2 + r.y;

		  if (xobj->value == i)
		  {
			  /* hili is better than shad.*/
			  XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
			  XFillRectangle(
				  dpy, xobj->win, xobj->gc,
				  inter.x, inter.y, inter.width, inter.height);
			  MyDrawString(
				  dpy, xobj, xobj->win, x, y, Title, fore, back,
				  shad, !xobj->flags[1], &r, evp);
		  }
		  else
		  {
			  XClearArea(
				  dpy, xobj->win,
				  inter.x, inter.y, inter.width, inter.height,
				  False);
			  MyDrawString(
				  dpy, xobj, xobj->win, x, y, Title, fore, hili,
				  back, !xobj->flags[1], &r, evp);
		  }
	  }
	  else
	  {
		  XClearArea(
			  dpy, xobj->win,
			  inter.x, inter.y, inter.width, inter.height,
			  False);
	  }
	  free(Title);
  }
}

void DestroyList(struct XObj *xobj)
{
  FlocaleUnloadFont(dpy, xobj->Ffont);
  XFreeGC(dpy, xobj->gc);
  XDestroyWindow(dpy, xobj->win);
}

void DrawList(struct XObj *xobj, XEvent *evp)
{
  int width = script_widget_dimension(xobj->width);
  int height = script_widget_dimension(xobj->height);
  int NbVisCell,NbCell;
  int HeightCell;
  XRectangle r;

  HeightCell = script_list_row_height(xobj->Ffont->height);
  NbVisCell = (height - 2 - 3*BdWidth) / HeightCell;
  NbCell = CountOption(xobj->title);
  xobj->value2 = script_list_first(xobj->value2, NbCell, NbVisCell);
  if (width < 5 || height < 9)
    return;
  /* Draw frame */
  DrawReliefRect(0, 0, width, height, xobj, hili, shad);
  if (width < 4 + 4 + 2*BdWidth + SbWidth + 4)
    return;

  /* Draw list frame */
  r.x = 2 + BdWidth;
  r.y = r.x;
  r.width = width - r.x - 4 - 2*BdWidth - SbWidth;
  r.height = height - r.y - 2*BdWidth;
  DrawReliefRect(r.x, r.y, r.width, r.height, xobj, shad, hili);

  /* Compute number of visible cells */
  NbVisCell = r.height/HeightCell;


  /* Draw cells */
  DrawCellule(
	  xobj, NbCell, NbVisCell, HeightCell,
	  /*FlocaleGetMinOffset(xobj->Ffont, ROTATION_0)*/
	  xobj->Ffont->ascent,
	  evp);

  /* Draw vertical scrollbar */
  DrawVSbList(xobj, NbCell, NbVisCell, 0);

}

void EvtMouseList(struct XObj *xobj, XButtonEvent *EvtButton)
{
  int width = script_widget_dimension(xobj->width);
  int height = script_widget_dimension(xobj->height);
  XRectangle rect,rectT;
  XPoint pt;
  int x1,y1,x2,y2;
  Window Win1,Win2;
  unsigned int modif;
  int In = 1;
  int NbVisCell,NbCell,HeightCell,NPosCell,PosMouse;
  int thumb_position, thumb_size, track;

  pt.x = EvtButton->x-xobj->x;
  pt.y = EvtButton->y-xobj->y;

  HeightCell = script_list_row_height(xobj->Ffont->height);
  NbVisCell = (height-6-BdWidth)/HeightCell;
  NbCell = CountOption(xobj->title);
  if (width < 42 || height < 15 || NbVisCell < 1)
    return;
  xobj->value2 = script_list_first(xobj->value2, NbCell, NbVisCell);

  /* Click on cell */
  rect.x = 4 + BdWidth;
  rect.y = rect.x;
  rect.width = width - rect.x - 10 - 2*BdWidth - SbWidth;
  rect.height = height -rect.y - 4 - 2*BdWidth;
  if(PtInRect(pt,rect))
  {
    /* Find cell */
    pt.y = pt.y - rect.y;
    NPosCell = script_list_cell(xobj->value2, pt.y/HeightCell, NbCell);
    if (NPosCell > CountOption(xobj->title))
      NPosCell = 0;
    else if (NPosCell >= (int64_t)xobj->value2 + NbVisCell)
    {
      NPosCell--;
    }
    if (NPosCell != xobj->value)
    {
      xobj->value = NPosCell;
      DrawList(xobj,NULL);
    }
    SendMsg(xobj, SingleClic);
    return ;
  }

  /* Click on vetical scrollbar top arrow */
  rect.y = 5 + BdWidth;
  rect.x = width - (6+BdWidth) - SbWidth+3;
  rect.height = 12;
  rect.width = 12;
  if(PtInRect(pt,rect))
  {
    DrawVSbList(xobj, NbCell, NbVisCell, 1);
    do
    {
      FQueryPointer(dpy, *xobj->ParentWin, &Win1, &Win2,
		    &x1, &y1, &x2, &y2, &modif);
      pt.y = y2- xobj->y;
      pt.x = x2- xobj->x;
      if (PtInRect(pt,rect))
      {
	if (In)
	{
	  if (!script_widget_wait_release(xobj, 8))
	    break;
	  xobj->value2--;
	  if (xobj->value2 < 1)
	    xobj->value2 = 1;
	  else
	  {
	    DrawCellule(xobj, NbCell, NbVisCell, HeightCell,
			xobj->Ffont->ascent, NULL);
	    DrawVSbList(xobj, NbCell, NbVisCell, 1);
	  }
	}
	else
	{
	  In = 1;
	  DrawVSbList(xobj, NbCell, NbVisCell, 1);
	  xobj->value2--;
	  if (xobj->value2 < 1)
	    xobj->value2 = 1;
	  else
	    DrawCellule(xobj, NbCell, NbVisCell, HeightCell,
			xobj->Ffont->ascent, NULL);
	}
      }
      else
      {
	if (In)
	{
	  In = 0;
	  DrawVSbList(xobj, NbCell, NbVisCell, 0);
	}
      }
    }
    while (script_widget_wait_release(xobj, 1));
    DrawVSbList(xobj, NbCell, NbVisCell, 0);
    return;
  }

  /* Click on vertical scrollbar bottom arrow */
  rect.y = height - 2*BdWidth -16;
  if(PtInRect(pt,rect))
  {
    DrawVSbList(xobj, NbCell, NbVisCell, 2);
    do
    {
      FQueryPointer(dpy, *xobj->ParentWin, &Win1, &Win2,
		    &x1, &y1, &x2, &y2, &modif);
      pt.y = y2 - xobj->y;
      pt.x = x2 - xobj->x;
      if (PtInRect(pt, rect))
      {
	if (In)
	{
	  if (!script_widget_wait_release(xobj, 8))
	    break;
	  if (xobj->value2 <= NbCell-NbVisCell)
	  {
	    xobj->value2++;
	    DrawCellule(xobj, NbCell, NbVisCell, HeightCell,
			xobj->Ffont->ascent, NULL);
	    DrawVSbList(xobj, NbCell, NbVisCell, 2);
	  }
	}
	else
	{
	  In = 1;
	  DrawVSbList(xobj, NbCell, NbVisCell, 2);
	  if (xobj->value2 <= NbCell-NbVisCell)
	  {
	    xobj->value2++;
	    DrawCellule(xobj, NbCell, NbVisCell, HeightCell,
			xobj->Ffont->ascent, NULL);
	  }
	}
      }
      else
      {
	if (In)
	{
	  In = 0;
	  DrawVSbList(xobj, NbCell, NbVisCell, 0);
	}
      }
    }
    while (script_widget_wait_release(xobj, 1));
    DrawVSbList(xobj, NbCell, NbVisCell, 0);
    return;
  }

  /* click on vertical scrollbar thumb zone */
  rect.y = 17 + BdWidth;
  rect.x = width - (6+BdWidth) - SbWidth + 2;
  track = height - rect.y - 19 - 2*BdWidth;
  if (track <= 0)
    return;
  rect.height = track;
  rect.width = SbWidth;
  if(PtInRect(pt,rect))
  {
    /* Click on thumb */
    rectT.x = rect.x;
    script_list_thumb(xobj->value2, NbCell, NbVisCell, track,
        &thumb_position, &thumb_size);
    rectT.y = rect.y + thumb_position;
    rectT.height = thumb_size;
    rectT.width = rect.width;
    if(PtInRect(pt,rectT))
    {
      PosMouse = pt.y - rectT.y - HeightCell / 2 + 2;
      do
      {
	FQueryPointer(dpy, *xobj->ParentWin, &Win1, &Win2,
		      &x1, &y1, &x2, &y2, &modif);
	/* Compute first cell */
	pt.y = y2-xobj->y - PosMouse;
	NPosCell = script_list_pointer_first(pt.y, rect.y, track,
	    NbCell, NbVisCell);
	if (xobj->value2 != NPosCell)
	{
	  xobj->value2 = NPosCell;
	  DrawCellule(
		  xobj, NbCell, NbVisCell, HeightCell, xobj->Ffont->ascent,
		  NULL);
	  DrawVSbList(xobj, NbCell, NbVisCell, 0);
	}
      }
      while (script_widget_wait_release(xobj, 1));
    }
    else if (pt.y < rectT.y)
    {
      NPosCell = script_list_move(xobj->value2, -NbVisCell, NbCell, NbVisCell);
      if (NPosCell < 1)
	NPosCell = 1;
      if (xobj->value2 != NPosCell)
      {
	xobj->value2 = NPosCell;
	DrawCellule(
		xobj, NbCell, NbVisCell, HeightCell, xobj->Ffont->ascent, NULL);
	DrawVSbList(xobj, NbCell, NbVisCell, 0);
      }
    }
    else if (pt.y > rectT.y + rectT.height)
    {
      NPosCell = script_list_move(xobj->value2, NbVisCell, NbCell, NbVisCell);
      if (NbCell>NbVisCell)
      {
	if (NPosCell > (NbCell - NbVisCell + 1))
	  NPosCell = NbCell - NbVisCell + 1;
      }
      else
	NPosCell = 1;
      if (xobj->value2 != NPosCell)
      {
	xobj->value2 = NPosCell;
	DrawCellule(
		xobj, NbCell, NbVisCell, HeightCell, xobj->Ffont->ascent, NULL);
	DrawVSbList(xobj, NbCell, NbVisCell, 0);
      }
    }
  }
}

void EvtKeyList(struct XObj *xobj, XKeyEvent *EvtKey)
{
  int width = script_widget_dimension(xobj->width);
  int height = script_widget_dimension(xobj->height);
  KeySym ks;
  unsigned char buf[10];
  int NbVisCell,NbCell,HeightCell,NPosCell;
  Window Win1,Win2;
  int x1,y1,x2,y2;
  unsigned int modif;
  XRectangle rect;
  XPoint pt;

  HeightCell = script_list_row_height(xobj->Ffont->height);
  NbVisCell = (height - 6 - BdWidth) / HeightCell;
  NbCell = CountOption(xobj->title);
  if (width < 42 || height < 15 || NbVisCell < 1)
    return;
  xobj->value2 = script_list_first(xobj->value2, NbCell, NbVisCell);
  FQueryPointer(dpy, xobj->win, &Win1, &Win2, &x1, &y1, &x2, &y2, &modif);
  pt.x = x2;
  pt.y = y2;
  rect.x = 4 + BdWidth;
  rect.y = rect.x;
  rect.width = width - rect.x -10 - 2*BdWidth - SbWidth;
  rect.height= height - rect.y - 4 - 2*BdWidth;

  /* Find char */
  XLookupString(EvtKey, (char *)buf, sizeof(buf), &ks, NULL);

  if (ks == XK_Return)
  {
    if(PtInRect(pt,rect))
    {
      /* Find cell */
      pt.y =  pt.y - rect.y;
      NPosCell = script_list_cell(xobj->value2, pt.y/HeightCell, NbCell);
      if (NPosCell > CountOption(xobj->title))
	NPosCell = 0;
      else if (NPosCell >= (int64_t)xobj->value2 + NbVisCell)
      {
	NPosCell--;
      }
      if (NPosCell != xobj->value)
      {
	xobj->value = NPosCell;
	DrawList(xobj,NULL);
      }
      SendMsg(xobj, SingleClic);
    }
  }
  else if (ks == XK_Up)
  {
    if (xobj->value2 == 1)
    {
      if(PtInRect(pt,rect))
      {
	/* Find cell */
	pt.y= pt.y - rect.y;
	NPosCell = script_list_cell(xobj->value2, pt.y/HeightCell, NbCell);
	if (NPosCell > CountOption(xobj->title))
	  NPosCell = 0;
	if (NPosCell > 1)
	{
	  FWarpPointer(dpy, None, None, 0, 0, 0, 0, 0, -HeightCell);
	}
      }
    }
    else
    {
      xobj->value2--;
      DrawCellule(
	      xobj, NbCell, NbVisCell, HeightCell, xobj->Ffont->ascent, NULL);
      DrawVSbList(xobj, NbCell, NbVisCell, 0);
    }
  }
  else if (ks == XK_Down)
  {
    if (xobj->value2 > NbCell-NbVisCell)
    {
      if(PtInRect(pt,rect))
      {
	/* Find cell */
	pt.y =  pt.y - rect.y;
	NPosCell = script_list_cell(xobj->value2, pt.y/HeightCell, NbCell);
	if (NPosCell > CountOption(xobj->title))
	  NPosCell = 0;
	if (NPosCell < NbCell && NPosCell > 0)
	{
	  FWarpPointer(dpy, None, None, 0, 0, 0, 0, 0, HeightCell);
	}
      }
    }
    else
    {
      xobj->value2++;
      DrawCellule(
	      xobj, NbCell, NbVisCell, HeightCell, xobj->Ffont->ascent, NULL);
      DrawVSbList(xobj, NbCell, NbVisCell, 0);
    }
  }
  else if (ks == XK_Prior && xobj->value2 > 1)
  {
    xobj->value2 = script_list_move(xobj->value2, -NbVisCell, NbCell, NbVisCell);
    if (xobj->value2 < 1)
      xobj->value2 = 1;
    DrawCellule(
	    xobj, NbCell, NbVisCell, HeightCell, xobj->Ffont->ascent, NULL);
    DrawVSbList(xobj, NbCell, NbVisCell, 0);
  }
  else if (ks == XK_Next && xobj->value2 <= NbCell-NbVisCell)
  {
    xobj->value2 = script_list_move(xobj->value2, NbVisCell, NbCell, NbVisCell);
    if (xobj->value2 > NbCell - NbVisCell + 1)
      xobj->value2 = NbCell - NbVisCell + 1;
    DrawCellule(
	    xobj, NbCell, NbVisCell, HeightCell, xobj->Ffont->ascent, NULL);
    DrawVSbList(xobj, NbCell, NbVisCell, 0);
  }
  else if (ks == XK_Home)
  {
    if (xobj->value2 > 1)
    {
      xobj->value2 = 1;
      DrawCellule(
	      xobj, NbCell, NbVisCell, HeightCell, xobj->Ffont->ascent, NULL);
      DrawVSbList(xobj, NbCell, NbVisCell, 0);
    }
  }
  else if (ks == XK_End)
  {
    if (xobj->value2 < NbCell - NbVisCell + 1)
    {
      xobj->value2 = NbCell - NbVisCell + 1;
      DrawCellule(
	      xobj, NbCell, NbVisCell, HeightCell, xobj->Ffont->ascent, NULL);
      DrawVSbList(xobj, NbCell, NbVisCell, 0);
    }
  }
}
