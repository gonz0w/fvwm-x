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

/* left, center and right offsets */
#define PUSH_BUTTON_LCR_OFFSETS 4,0,4

/*
 * Function for PushButton
 */
void InitPushButton(struct XObj *xobj)
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
	if (!x11base->cursor) {
		Attr.cursor = XCreateFontCursor(dpy,XC_hand2);
		mask |= CWCursor;  /* Window cursor */
	}
	Attr.background_pixel = xobj->TabColor[back];
	mask |= CWBackPixel;


	/* Window thickness = 0 */
	xobj->win = XCreateWindow(
		dpy, *xobj->ParentWin, xobj->x, xobj->y, xobj->width,
		xobj->height, 0, CopyFromParent, InputOutput, CopyFromParent,
		mask, &Attr);
		if (mask & CWCursor)
		  XFreeCursor(dpy, Attr.cursor);
	xobj->gc = fvwmlib_XCreateGC(dpy, xobj->win, 0, NULL);
	XSetForeground(dpy, xobj->gc, xobj->TabColor[fore]);

	if (xobj->Ffont->font != NULL)
		XSetFont(dpy, xobj->gc, xobj->Ffont->font->fid);

	XSetLineAttributes(dpy, xobj->gc, 1, LineSolid, CapRound, JoinMiter);

	if (xobj->colorset >= 0)
		SetWindowBackground(dpy, xobj->win, xobj->width, xobj->height,
				    GetColorset(xobj->colorset), Pdepth,
				    xobj->gc, True);
	xobj->value3 = CountOption(xobj->title);
	XSelectInput(dpy, xobj->win, ExposureMask);
}

void DestroyPushButton(struct XObj *xobj)
{
	FlocaleUnloadFont(dpy, xobj->Ffont);
	XFreeGC(dpy, xobj->gc);
	XDestroyWindow(dpy, xobj->win);
}


void DrawPushButton(struct XObj *xobj, XEvent *evp)
{
	DrawReliefRect(0, 0, xobj->width, xobj->height, xobj, hili, shad);
	DrawIconStr(0, xobj, True, PUSH_BUTTON_LCR_OFFSETS, NULL, NULL, evp);
}

void EvtMousePushButton(struct XObj *xobj, XButtonEvent *EvtButton)
{
	XEvent event;
	int End = 1;
	unsigned int modif;
	int x1,x2,y1,y2;
	Window Win1,Win2,WinPop;
	Window WinBut = 0;
	int In = 0;
	int x,y,hOpt,hMenu,wMenu;
	int oldvalue = 0,newvalue = 0;
	unsigned long mask;
	XSetWindowAttributes Attr;

	if (EvtButton->button == Button1)
	{

		while (End)
		{
			if (!script_widget_next_event(xobj, &event))
			{
				DrawPushButton(xobj, NULL);
				return;
			}
			switch (event.type)
			{
			case EnterNotify:
				FQueryPointer(
					dpy, *xobj->ParentWin,
					&Win1, &Win2, &x1, &y1, &x2, &y2,
					&modif);
				if (WinBut == 0)
				{
					WinBut = Win2;
					DrawReliefRect(
						0, 0, xobj->width,
						xobj->height, xobj, shad,
						hili);
					DrawIconStr(
						0,xobj, True,
						PUSH_BUTTON_LCR_OFFSETS, NULL,
						NULL, NULL);
					In = 1;
				}
				else
				{
					if (Win2 == WinBut)
					{
						DrawReliefRect(
							0, 0, xobj->width,
							xobj->height, xobj,
							shad, hili);
						DrawIconStr(
							1, xobj, True,
							PUSH_BUTTON_LCR_OFFSETS,
							NULL, NULL, NULL);
						In = 1;
					}
					else if (In)
					{
						In = 0;
						DrawReliefRect(
							0, 0, xobj->width,
							xobj->height, xobj,
							hili, shad);
						DrawIconStr(
							0, xobj, True,
							PUSH_BUTTON_LCR_OFFSETS,
							NULL, NULL, NULL);
					}
				}
				break;
			case LeaveNotify:
				FQueryPointer(
					dpy, *xobj->ParentWin,
					&Win1, &Win2, &x1, &y1, &x2, &y2,
					&modif);
				if (Win2 == WinBut)
				{
					In = 1;
					DrawReliefRect(
						0, 0, xobj->width,
						xobj->height, xobj, shad,
						hili);
					DrawIconStr(
						1, xobj, True,
						PUSH_BUTTON_LCR_OFFSETS, NULL,
						NULL, NULL);
				}
				else if (In)
				{
					DrawReliefRect(
						0, 0, xobj->width,
						xobj->height, xobj, hili,
						shad);
					DrawIconStr(
						0, xobj, True,
						PUSH_BUTTON_LCR_OFFSETS, NULL,
						NULL, NULL);
					In = 0;
				}
				break;
			case ButtonRelease:
				End = 0;
				DrawReliefRect(
					0, 0, xobj->width, xobj->height, xobj,
					hili, shad);
				DrawIconStr(
					0, xobj, True,
					PUSH_BUTTON_LCR_OFFSETS, NULL, NULL,
					NULL);
				if (In)
				{
					/* Send an empty message of SingleClick type
					 * for a mouse click */
					xobj->value = 1;
					SendMsg(xobj, SingleClic);
					xobj->value = 0;
				}
				break;
			}
		}
	}
	/* Drawing the popup menu */
	else if (EvtButton->button == Button3)
	{
		if (xobj->value3 > 1)
		{
			script_layout_geometry popup;
			if (!script_widget_popup(xobj, 2, 34, false, &popup, &hOpt))
			{
			  script_geometry_diagnostic(__func__, xobj->id);
			  return;
			}
			xobj->value3 = CountOption(xobj->title);
			x = popup.x;
			y = popup.y;
			hMenu = popup.height;
			wMenu = popup.width;
			mask = 0;
			Attr.background_pixel = xobj->TabColor[back];
			mask |= CWBackPixel;
			Attr.border_pixel  =  0;
			mask |= CWBorderPixel;
			Attr.colormap = Pcmap;
			mask |= CWColormap;
			if (!x11base->cursor) {
				Attr.cursor = XCreateFontCursor(dpy,XC_hand2);
				mask |= CWCursor;  /* Window cursor */
			}
			Attr.override_redirect = True;
			mask |= CWOverrideRedirect;
			WinPop = XCreateWindow(
				dpy, Root, x, y, wMenu-5, hMenu, 0,
				Pdepth, InputOutput, Pvisual, mask, &Attr);
				if (mask & CWCursor)
				  XFreeCursor(dpy, Attr.cursor);
			if (xobj->colorset >= 0)
				SetWindowBackground(
					dpy, WinPop, wMenu - 5, hMenu,
					GetColorset(xobj->colorset), Pdepth,
					xobj->gc, True);
			XMapRaised(dpy, WinPop);

			/* Draw the menu */
			DrawPMenu(xobj, WinPop, hOpt, 1);
			do
			{
				FQueryPointer(
					dpy, Root, &Win1, &Win2, &x1, &y1,
					&x2, &y2, &modif);
				/* Find current option */
				y2 = y2 - y;
				x2 = x2 - x;
				{
					/* Compute xobj->value */
					if ((x2 > 0) && (x2 < wMenu) &&
					    (y2 > 0) && (y2 < hMenu))
						newvalue = y2 / hOpt+1;
					else
						newvalue = 0;
					if (newvalue!=oldvalue)
					{
						UnselectMenu(
							xobj, WinPop, hOpt,
							oldvalue, wMenu-5,
							xobj->Ffont->ascent,
							1);
						SelectMenu(
							xobj, WinPop, hOpt,
							newvalue);
						oldvalue = newvalue;
					}
				}
			}
			while (FvwmScriptModalEvent(xobj, &event, 20) &&
			       event.type != ButtonRelease && event.type != KeyPress);
			if (event.type != ButtonRelease)
				newvalue = 0;
			XDestroyWindow(dpy, WinPop);
			if (newvalue != 0)
			{
				xobj->value = newvalue;
				SendMsg(xobj, SingleClic);
				xobj->value = 0;
			}
			xobj->DrawObj(xobj, NULL);
		} /* xobj->value3 > 1 */
	} /* EvtButton->button == Button3 */
}

void EvtKeyPushButton(struct XObj *xobj, XKeyEvent *EvtKey)
{
	char car[11];
	char *carks2;
	char *carks;
	KeySym ks;
	int Size;

	Size = XLookupString(EvtKey, car, 10, &ks, NULL);
	car[Size] = '\0';
	carks2 = XKeysymToString(ks);
	if (carks2 != NULL)
	{
		carks = fxstrdup(carks2);
		if (strcmp(carks, "Return") == 0)
		{
			xobj->value = 1;
			SendMsg(xobj, SingleClic);
			xobj->value = 0;
		}
		free(carks);
	}
}
