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

/*
 * File containing functions for graphical objects
 */

#include "config.h"

#include "libs/log.h"
#include "Tools.h"

extern void InitPushButton(struct XObj *xobj);
extern void DrawPushButton(struct XObj *xobj, XEvent *evp);
extern void EvtMousePushButton(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyPushButton(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitRadioButton(struct XObj *xobj);
extern void DrawRadioButton(struct XObj *xobj, XEvent *evp);
extern void EvtMouseRadioButton(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyRadioButton(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitItemDraw(struct XObj *xobj);
extern void DrawItemDraw(struct XObj *xobj, XEvent *evp);
extern void EvtMouseItemDraw(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyItemDraw(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitCheckBox(struct XObj *xobj);
extern void DrawCheckBox(struct XObj *xobj, XEvent *evp);
extern void EvtMouseCheckBox(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyCheckBox(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitTextField(struct XObj *xobj);
extern void DrawTextField(struct XObj *xobj, XEvent *evp);
extern void EvtMouseTextField(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyTextField(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitHScrollBar(struct XObj *xobj);
extern void DrawHScrollBar(struct XObj *xobj, XEvent *evp);
extern void EvtMouseHScrollBar(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyHScrollBar(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitVScrollBar(struct XObj *xobj);
extern void DrawVScrollBar(struct XObj *xobj, XEvent *evp);
extern void EvtMouseVScrollBar(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyVScrollBar(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitPopupMenu(struct XObj *xobj);
extern void DrawPopupMenu(struct XObj *xobj, XEvent *evp);
extern void EvtMousePopupMenu(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyPopupMenu(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitRectangle(struct XObj *xobj);
extern void DrawRectangle(struct XObj *xobj, XEvent *evp);
extern void EvtMouseRectangle(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyRectangle(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitMiniScroll(struct XObj *xobj);
extern void DrawMiniScroll(struct XObj *xobj, XEvent *evp);
extern void EvtMouseMiniScroll(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyMiniScroll(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitSwallow(struct XObj *xobj);
extern void DestroySwallow(struct XObj *xobj);
extern void DrawSwallow(struct XObj *xobj, XEvent *evp);
extern void EvtMouseSwallow(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeySwallow(struct XObj *xobj,XKeyEvent *EvtKey);
extern void ProcessMsgSwallow(
	struct XObj *xobj,unsigned long type,unsigned long *body);

extern void InitHDipstick(struct XObj *xobj);
extern void DrawHDipstick(struct XObj *xobj, XEvent *evp);
extern void EvtMouseHDipstick(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyHDipstick(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitVDipstick(struct XObj *xobj);
extern void DrawVDipstick(struct XObj *xobj, XEvent *evp);
extern void EvtMouseVDipstick(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyVDipstick(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitList(struct XObj *xobj);
extern void DrawList(struct XObj *xobj, XEvent *evp);
extern void EvtMouseList(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyList(struct XObj *xobj,XKeyEvent *EvtKey);

extern void InitMenu(struct XObj *xobj);
extern void DrawMenu(struct XObj *xobj, XEvent *evp);
extern void EvtMouseMenu(struct XObj *xobj,XButtonEvent *EvtButton);
extern void EvtKeyMenu(struct XObj *xobj,XKeyEvent *EvtKey);

/* Ordinary widgets replace the borrowed base GC during initialization. */
static void script_widget_destroy_window(struct XObj *xobj)
{
	if (xobj->gc != NULL)
		XFreeGC(dpy, xobj->gc);
	if (xobj->win != None)
		XDestroyWindow(dpy, xobj->win);
	xobj->gc = NULL;
	xobj->win = None;
}

void script_widget_release(struct XObj **slot, bool initialized)
{
	struct XObj *xobj = *slot;

	if (xobj == NULL)
		return;
	/* Callbacks finish first: they may inspect other widgets.
	 * A partial widget borrows the base GC and owns no named colors. */
	if (xobj->Ffont != NULL)
		FlocaleUnloadFont(dpy, xobj->Ffont);
	if (initialized && xobj->colorset < 0)
	{
		for (int i = 0; i < 4; i++)
			PictureFreeColors(dpy, Pcmap, &xobj->TabColor[i],
				1, 0, True);
	}
	script_icon_clear(xobj);
	free(xobj->title);
	free(xobj->swallow);
	free(xobj->icon);
	free(xobj->font);
	free(xobj->forecolor);
	free(xobj->backcolor);
	free(xobj->shadcolor);
	free(xobj->hilicolor);
	free(xobj);
	*slot = NULL;
}

/*
 * Choose function depending on button type
 */

void ChooseFunction(struct XObj *xobj,char *type)
{
	/* Only widgets with a packet consumer install this optional callback. */
	xobj->ProcessMsg = NULL;
	if (strcmp(type,"PushButton")==0)
	{
		xobj->TypeWidget=PushButton;
		xobj->InitObj=InitPushButton;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawPushButton;
		xobj->EvtMouse=EvtMousePushButton;
		xobj->EvtKey=EvtKeyPushButton;
	}
	else if (strcmp(type,"RadioButton")==0)
	{
		xobj->TypeWidget=RadioButton;
		xobj->InitObj=InitRadioButton;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawRadioButton;
		xobj->EvtMouse=EvtMouseRadioButton;
		xobj->EvtKey=EvtKeyRadioButton;
	}
	else if (strcmp(type,"ItemDraw")==0)
	{
		xobj->TypeWidget=ItemDraw;
		xobj->InitObj=InitItemDraw;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawItemDraw;
		xobj->EvtMouse=EvtMouseItemDraw;
		xobj->EvtKey=EvtKeyItemDraw;
	}
	else if (strcmp(type,"CheckBox")==0)
	{
		xobj->TypeWidget=CheckBox;
		xobj->InitObj=InitCheckBox;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawCheckBox;
		xobj->EvtMouse=EvtMouseCheckBox;
		xobj->EvtKey=EvtKeyCheckBox;
	}
	else if (strcmp(type,"TextField")==0)
	{
		xobj->TypeWidget=TextField;
		xobj->InitObj=InitTextField;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawTextField;
		xobj->EvtMouse=EvtMouseTextField;
		xobj->EvtKey=EvtKeyTextField;
	}
	else if (strcmp(type,"HScrollBar")==0)
	{
		xobj->TypeWidget=HScrollBar;
		xobj->InitObj=InitHScrollBar;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawHScrollBar;
		xobj->EvtMouse=EvtMouseHScrollBar;
		xobj->EvtKey=EvtKeyHScrollBar;
	}
	else if (strcmp(type,"VScrollBar")==0)
	{
		xobj->TypeWidget=VScrollBar;
		xobj->InitObj=InitVScrollBar;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawVScrollBar;
		xobj->EvtMouse=EvtMouseVScrollBar;
		xobj->EvtKey=EvtKeyVScrollBar;
	}
	else if (strcmp(type,"PopupMenu")==0)
	{
		xobj->TypeWidget=PopupMenu;
		xobj->InitObj=InitPopupMenu;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawPopupMenu;
		xobj->EvtMouse=EvtMousePopupMenu;
		xobj->EvtKey=EvtKeyPopupMenu;
	}
	else if (strcmp(type,"Rectangle")==0)
	{
		xobj->TypeWidget=Rectangle;
		xobj->InitObj=InitRectangle;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawRectangle;
		xobj->EvtMouse=EvtMouseRectangle;
		xobj->EvtKey=EvtKeyRectangle;
	}
	else if (strcmp(type,"MiniScroll")==0)
	{
		xobj->TypeWidget=MiniScroll;
		xobj->InitObj=InitMiniScroll;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawMiniScroll;
		xobj->EvtMouse=EvtMouseMiniScroll;
		xobj->EvtKey=EvtKeyMiniScroll;
	}
	else if (strcmp(type,"SwallowExec")==0)
	{
		xobj->TypeWidget=SwallowExec;
		xobj->InitObj=InitSwallow;
		xobj->DestroyObj=DestroySwallow;
		xobj->DrawObj=DrawSwallow;
		xobj->EvtMouse=EvtMouseSwallow;
		xobj->EvtKey=EvtKeySwallow;
		xobj->ProcessMsg=ProcessMsgSwallow;
	}
	else if (strcmp(type,"HDipstick")==0)
	{
		xobj->TypeWidget=HDipstick;
		xobj->InitObj=InitHDipstick;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawHDipstick;
		xobj->EvtMouse=EvtMouseHDipstick;
		xobj->EvtKey=EvtKeyHDipstick;
	}
	else if (strcmp(type,"VDipstick")==0)
	{
		xobj->TypeWidget=VDipstick;
		xobj->InitObj=InitVDipstick;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawVDipstick;
		xobj->EvtMouse=EvtMouseVDipstick;
		xobj->EvtKey=EvtKeyVDipstick;
	}
	else if (strcmp(type,"List")==0)
	{
		xobj->TypeWidget=List;
		xobj->InitObj=InitList;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawList;
		xobj->EvtMouse=EvtMouseList;
		xobj->EvtKey=EvtKeyList;
	}
	else if (strcmp(type,"Menu")==0)
	{
		xobj->TypeWidget=Menu;
		xobj->InitObj=InitMenu;
		xobj->DestroyObj=script_widget_destroy_window;
		xobj->DrawObj=DrawMenu;
		xobj->EvtMouse=EvtMouseMenu;
		xobj->EvtKey=EvtKeyMenu;
	}
	else
	{
		fvwm_debug(__func__, "Item %s unknown\n",type);
		exit(1);
	}
}
