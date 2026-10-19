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
#include <limits.h>

#include "libs/flocale_font_types.h"
#include "libs/ColorUtils.h"
#include "libs/Graphics.h"
#include "libs/FEvent.h"
#include "libs/FSelection.h"
#include "libs/log.h"
#include "Tools.h"
#include "../script_storage.h"

static Bool text_field_edit(struct XObj *xobj, const char *text,
	size_t length, Bool replace_selection);

static void text_field_normalize(struct XObj *xobj)
{
	int length = getCharOffsetBoundsCheck(xobj->Ffont, xobj->title, INT_MAX);

	if (xobj->value < 0)
		xobj->value = 0;
	else if (xobj->value > length)
		xobj->value = length;
	if (xobj->value2 < 0)
		xobj->value2 = 0;
	else if (xobj->value2 > length)
		xobj->value2 = length;
	if (xobj->value3 < 0)
		xobj->value3 = 0;
	else if (xobj->value3 > xobj->value)
		xobj->value3 = xobj->value;
}

/*
 * Function for TextField
 */

void InitTextField(struct XObj *xobj)
{
	unsigned long mask;
	XSetWindowAttributes Attr;
	int num_chars;

	/* Save colors and fonts */
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

	mask=0;
	Attr.cursor=XCreateFontCursor(dpy,XC_xterm);
	mask|=CWCursor; /* window cursor */
	Attr.background_pixel=xobj->TabColor[back];
	mask|=CWBackPixel;

	xobj->win=XCreateWindow(dpy,*xobj->ParentWin,
				xobj->x,xobj->y,xobj->width,xobj->height,0,
				CopyFromParent,InputOutput,CopyFromParent,
				mask,&Attr);
	xobj->gc=fvwmlib_XCreateGC(dpy,xobj->win,0,NULL);
	XSetForeground(dpy,xobj->gc,xobj->TabColor[fore]);

	if (xobj->Ffont->font != NULL)
		XSetFont(dpy, xobj->gc, xobj->Ffont->font->fid);

	XSetLineAttributes(dpy,xobj->gc,1,LineSolid,CapRound,JoinMiter);
	/* value2 gives the end of the selected zone */
	/* calculate number of characters in title */
	num_chars = getCharOffsetBoundsCheck(xobj->Ffont, xobj->title, INT_MAX);
	if (xobj->value < 0)
		xobj->value = 0;
	if (xobj->value > num_chars)
		xobj->value = num_chars;
	xobj->value2=xobj->value;
	/* left position of the visible title */
	xobj->value3=0;

	if (xobj->colorset >= 0)
		SetWindowBackground(dpy, xobj->win, xobj->width, xobj->height,
				    GetColorset(xobj->colorset), Pdepth,
				    xobj->gc, True);
	XSelectInput(dpy, xobj->win, ExposureMask);
}

void DestroyTextField(struct XObj *xobj)
{
	FlocaleUnloadFont(dpy,xobj->Ffont);
	XFreeGC(dpy,xobj->gc);
	XDestroyWindow(dpy,xobj->win);
}

/* text cursor drawing */
void DrawPointTxt(struct XObj *xobj,unsigned int pixel)
{
#define dec 2
	int x,y;
	int offset_abs,offset_rel;
	XSegment segm[2];

	/* calculate byte offsets */
	text_field_normalize(xobj);
	offset_abs = getByteOffsetBoundsCheck(xobj->Ffont,xobj->title,
						   xobj->value);
	offset_rel = getByteOffsetBoundsCheck(xobj->Ffont,xobj->title,
						   xobj->value3);
	x = FlocaleTextWidth(xobj->Ffont, xobj->title + offset_rel,
			     offset_abs - offset_rel)+5;
	y = xobj->Ffont->ascent + 5;

	segm[0].x1=x;
	segm[0].y1=y;
	segm[0].x2=x-dec;
	segm[0].y2=y+dec;
	segm[1].x1=x;
	segm[1].y1=y;
	segm[1].x2=x+dec;
	segm[1].y2=y+dec;
	XSetForeground(dpy,xobj->gc,pixel);
	XDrawSegments(dpy,xobj->win,xobj->gc,segm,2);
}

/* text field drawing */

void DrawTextField(struct XObj *xobj, XEvent *evp)
{
	int length, cursor, first, last, left, right, previous;
	int width = script_widget_dimension(xobj->width);
	int height = script_widget_dimension(xobj->height);
	int x, selected_width;

	text_field_normalize(xobj);
	if (width < 4 || height < 4)
		return;
	length = strlen(xobj->title);
	cursor = getByteOffsetBoundsCheck(xobj->Ffont, xobj->title, xobj->value);
	left = getByteOffsetBoundsCheck(xobj->Ffont, xobj->title, xobj->value3);
	/* Stop at the cursor: no negative text length reaches the font API. */
	while (left < cursor && FlocaleTextWidth(xobj->Ffont,
	    xobj->title + left, cursor - left) > width - 10)
	{
		left += script_locale_text_next(xobj->Ffont, xobj->title + left,
			length - left);
	}
	xobj->value3 = getCharOffsetBoundsCheck(xobj->Ffont, xobj->title, left);
	right = previous = left;
	while (right < length && FlocaleTextWidth(xobj->Ffont,
	    xobj->title + left, right - left) <= width - 10)
	{
		previous = right;
		right += script_locale_text_next(xobj->Ffont, xobj->title + right,
			length - right);
	}
	if (FlocaleTextWidth(xobj->Ffont, xobj->title + left,
	    right - left) > width - 10)
		right = previous;

	DrawReliefRect(0, 0, width, height, xobj, shad, hili);
	XClearArea(dpy, xobj->win, 2, 2, width - 4, height - 4, False);
	XSetForeground(dpy, xobj->gc, xobj->TabColor[fore]);
	FwinString->win = xobj->win;
	FwinString->gc = xobj->gc;
	FwinString->x = 5;
	FwinString->y = xobj->Ffont->ascent + 5;
	FwinString->str = xobj->title + left;
	FwinString->len = right - left;
	FlocaleDrawString(dpy, xobj->Ffont, FwinString, FWS_HAVE_LENGTH);

	first = getByteOffsetBoundsCheck(xobj->Ffont, xobj->title, xobj->value);
	last = getByteOffsetBoundsCheck(xobj->Ffont, xobj->title, xobj->value2);
	if (first > last)
	{
		int swap = first;
		first = last;
		last = swap;
	}
	if (first < left)
		first = left;
	if (last > right)
		last = right;
	if (last > first && xobj->Ffont->height > 2 && height > 7)
	{
		x = FlocaleTextWidth(xobj->Ffont, xobj->title + left, first - left);
		selected_width = FlocaleTextWidth(xobj->Ffont,
			xobj->title + first, last - first);
		XSetFunction(dpy, xobj->gc, GXinvert);
		XFillRectangle(dpy, xobj->win, xobj->gc, x + 5, 7,
			selected_width, script_widget_dimension(
				(int64_t)xobj->Ffont->height - 2));
		XSetFunction(dpy, xobj->gc, GXcopy);
	}
	DrawPointTxt(xobj, xobj->TabColor[fore]);
}

static int text_field_pointer(struct XObj *xobj, int pointer_x)
{
	int left = getByteOffsetBoundsCheck(xobj->Ffont, xobj->title,
		xobj->value3);
	int length = strlen(xobj->title);
	int byte = left;
	int characters = xobj->value3;

	while (byte < length && pointer_x > FlocaleTextWidth(xobj->Ffont,
	    xobj->title + left, byte - left) + 8)
	{
		byte += script_locale_text_next(xobj->Ffont, xobj->title + byte,
			length - byte);
		characters++;
	}
	return characters;
}

static void text_field_save_selection(struct XObj *xobj, Time time)
{
	char *text = GetText(xobj, xobj->value2);
	int length = strlen(text);
	int i;

	for (i = 0; i <= 7; i++)
		XStoreBuffer(dpy, text, length, i);
	free(Scrapt);
	Scrapt = text;
	x11base->HaveXSelection = True;
	XSetSelectionOwner(dpy, XA_PRIMARY, x11base->win, time);
}

void EvtMouseTextField(struct XObj *xobj, XButtonEvent *EvtButton)
{
	unsigned int modifiers;
	int root_x, root_y, x, y, position;
	Window root, child;
	XEvent event;

	text_field_normalize(xobj);
	if (EvtButton->button == Button2)
	{
		if (!x11base->HaveXSelection)
		{
			char *received = FSelectionRequestTextWithService(dpy, XA_PRIMARY,
				XA_STRING, propriete, EvtButton->time,
				FSELECTION_TIMEOUT_MS, FvwmScriptModalService, xobj);
			if (received == NULL)
				return;
			free(Scrapt);
			Scrapt = received;
		}
		if (Scrapt[0] != '\0')
		{
			if (!text_field_edit(xobj, Scrapt, strlen(Scrapt), False))
				return;
			DrawTextField(xobj, NULL);
			SendMsg(xobj, SingleClic);
		}
		return;
	}
	if (EvtButton->button != Button1 && EvtButton->button != Button3)
		return;
	FQueryPointer(dpy, *xobj->ParentWin, &root, &child,
		&root_x, &root_y, &x, &y, &modifiers);
	position = text_field_pointer(xobj, x - xobj->x);
	if (EvtButton->button == Button1)
		xobj->value = position;
	else if ((position < xobj->value && xobj->value < xobj->value2) ||
	    (position > xobj->value && xobj->value > xobj->value2))
		xobj->value = xobj->value2;
	xobj->value2 = position;
	DrawTextField(xobj, NULL);
	for (;;)
	{
		if (!script_widget_next_event(xobj, &event))
			return;
		if (event.type == ButtonRelease)
			break;
		if (event.type != MotionNotify)
			continue;
		FQueryPointer(dpy, *xobj->ParentWin, &root, &child,
			&root_x, &root_y, &x, &y, &modifiers);
		xobj->value2 = text_field_pointer(xobj, x - xobj->x);
		DrawTextField(xobj, NULL);
	}
	if (xobj->value != xobj->value2)
	{
		text_field_save_selection(xobj, EvtButton->time);
		if (EvtButton->button == Button1)
			SelectOneTextField(xobj);
	}
}

static Bool delete_keyboard_selection(struct XObj *xobj)
{
	int first, last, first_byte, last_byte;

	if (xobj->value == xobj->value2)
		return False;
	first = xobj->value < xobj->value2 ? xobj->value : xobj->value2;
	last = xobj->value > xobj->value2 ? xobj->value : xobj->value2;
	first_byte = getByteOffsetBoundsCheck(xobj->Ffont, xobj->title, first);
	last_byte = getByteOffsetBoundsCheck(xobj->Ffont, xobj->title, last);
	script_text_delete(xobj->title, first_byte, last_byte);
	xobj->value = xobj->value2 = first;
	if (xobj->value3 > first)
		xobj->value3 = first;
	return True;
}

static Bool text_field_edit(struct XObj *xobj, const char *text,
	size_t length, Bool replace_selection)
{
	int first = getByteOffsetBoundsCheck(xobj->Ffont, xobj->title,
		xobj->value);
	int last = replace_selection ? getByteOffsetBoundsCheck(xobj->Ffont,
		xobj->title, xobj->value2) : first;

	if (first > last)
	{
		int swap = first;
		first = last;
		last = swap;
	}
	if (!script_text_edit(&xobj->title, first, last, text, length,
	    script_storage_runtime()->value_limit))
	{
		fvwm_debug(__func__,
			"Text edit refused (ValueLimit=%zu or allocation failure); "
			"title and selection unchanged",
			script_storage_runtime()->value_limit);
		return False;
	}
	xobj->value = getCharOffsetBoundsCheck(xobj->Ffont, xobj->title,
		first + (int)length);
	xobj->value2 = xobj->value;
	return True;
}

void EvtKeyTextField(struct XObj *xobj,XKeyEvent *EvtKey)
{
	int i;
	char car[11];
	char* carks2;
	char* carks = NULL;
	KeySym ks;
	int NewPos;
	int new_value;

	text_field_normalize(xobj);
	/* Find character */
	i=XLookupString(EvtKey,car,10,&ks,NULL);
	if (FvwmScriptUsesKeyboardFocus())
	{
		/* This editor policy is opt-in; old scripts keep their input rules. */
		if (IsModifierKey(ks))
			return;
		if ((EvtKey->state & ControlMask) && (ks == XK_a || ks == XK_A))
		{
			xobj->value = 0;
			xobj->value2 = getCharOffsetBoundsCheck(xobj->Ffont, xobj->title, INT_MAX);
			DrawTextField(xobj, NULL);
			return;
		}
		if ((EvtKey->state & ControlMask) && (ks == XK_v || ks == XK_V))
		{
			Atom clipboard = XInternAtom(dpy, "CLIPBOARD", False);
			Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
			char *text = FSelectionRequestTextWithService(dpy, clipboard, utf8,
				propriete, EvtKey->time, FSELECTION_TIMEOUT_MS,
				FvwmScriptModalService, xobj);
			int byte;

			if (text == NULL)
				text = FSelectionRequestTextWithService(dpy, clipboard, XA_STRING,
					propriete, EvtKey->time, FSELECTION_TIMEOUT_MS,
				FvwmScriptModalService, xobj);
			if (text == NULL)
				return;
			for (byte = 0; text[byte] != '\0'; byte++)
				if ((unsigned char)text[byte] < ' ')
					text[byte] = ' ';
			if (!text_field_edit(xobj, text, strlen(text), True))
			{
				free(text);
				return;
			}
			free(text);
			DrawTextField(xobj, NULL);
			SendMsg(xobj, SingleClic);
			return;
		}
		if ((ks == XK_BackSpace || ks == XK_Delete ||
		     (i > 0 && !(EvtKey->state & ControlMask))) &&
		    xobj->value != xobj->value2)
		{
			if (ks == XK_BackSpace || ks == XK_Delete)
			{
				delete_keyboard_selection(xobj);
				DrawTextField(xobj, NULL);
				SendMsg(xobj, SingleClic);
				return;
			}
			if (text_field_edit(xobj, car, i, True))
			{
				DrawTextField(xobj, NULL);
				SendMsg(xobj, SingleClic);
			}
			return;
		}
		if (ks == XK_Home || ks == XK_End)
		{
			xobj->value = ks == XK_Home ? 0 :
				getCharOffsetBoundsCheck(xobj->Ffont, xobj->title, INT_MAX);
			xobj->value2 = xobj->value;
			DrawTextField(xobj, NULL);
			return;
		}
		if (ks == XK_Delete)
		{
			int offset = getByteOffsetBoundsCheck(xobj->Ffont,
				xobj->title, xobj->value);
			if (offset < strlen(xobj->title))
			{
				int size = script_locale_text_next(xobj->Ffont,
					xobj->title + offset, strlen(xobj->title) - offset);
				script_text_delete(xobj->title, offset, offset + size);
				DrawTextField(xobj, NULL);
				SendMsg(xobj, SingleClic);
			}
			return;
		}
		if ((EvtKey->state & ControlMask) && i > 0)
			return;
	}
	/* calculate byte offset from current position */
	NewPos = getByteOffsetBoundsCheck(xobj->Ffont, xobj->title,
					  xobj->value);
	car[i]='\0';
	carks2=XKeysymToString(ks);

	if (carks2!=NULL)
	{
		carks=fxstrdup(carks2);
		if (strcmp(carks,"Right")==0)
		{
			/* if we are at the end, don't go further */
			if (NewPos >= strlen(xobj->title))
				NewPos = strlen(xobj->title);
			/* otherwise step forward as many bytes as the
			   next charcter at the insertion point is wide */
			else
				NewPos += script_locale_text_next(xobj->Ffont,
					xobj->title + NewPos, strlen(xobj->title) - NewPos);
		}
		else if (strcmp(carks,"Left")==0)
		{
			/* if we are at the beginning, don't go further */
			if (NewPos <= 0)
				NewPos=0;
			/* otherwise step back as many bytes as the chacter
			   behind the insertion point */
			else
				NewPos =
					getByteOffsetBoundsCheck(
						   xobj->Ffont,
						   xobj->title,
						   xobj->value - 1);
		}
		else if (strcmp(carks,"Return")==0)
		{
			;
		}
		else if ((strcmp(carks,"Delete")==0)||
			 (strcmp(carks,"BackSpace")==0))
		{
			if (NewPos>0)
			{
				/* need to delete the "right" amount of
				   bytes from the string, we might be using
				   multi-byte strings */
				int PrevPos;
				PrevPos = getByteOffsetBoundsCheck(xobj->Ffont,
					xobj->title, xobj->value - 1);
				script_text_delete(xobj->title, PrevPos, NewPos);
				NewPos = PrevPos;
				SendMsg(xobj,SingleClic);
			}
		}
		else if (i!=0)        /* Normal character */
		{
			int old_value = xobj->value;
			int old_selection = xobj->value2;

			if (!text_field_edit(xobj, car, i, False))
			{
				free(carks);
				return;
			}
			NewPos = getByteOffsetBoundsCheck(xobj->Ffont,
				xobj->title, xobj->value);
			/* Legacy callbacks see the old indices and edited title. */
			xobj->value = old_value;
			xobj->value2 = old_selection;
			SendMsg(xobj,SingleClic);
		}

	}

	new_value = getCharOffsetBoundsCheck(xobj->Ffont, xobj->title, NewPos);
	if ((xobj->value != new_value) || (xobj->value2 != new_value))
	{

		/* convert back to char offsets */
		xobj->value = new_value;
		/*FlocaleStringByteToCharOffset(xobj->Ffont,
							  xobj->title,
							  NewPos);*/
		xobj->value2 = new_value; /*FlocaleStringByteToCharOffset(xobj->Ffont,
							  xobj->title,
							  NewPos);*/
		DrawPointTxt(xobj,xobj->TabColor[fore]);
		DrawTextField(xobj,NULL);
	}
	free(carks);
}
