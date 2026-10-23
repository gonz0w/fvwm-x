/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "libs/fvwmlib.h"
#include "libs/Colorset.h"
#include "FvwmForm.h"
#include "libs/flocale_font_types.h"

/* One child window, with draw work bounded by the configured viewport. */
void form_list_draw(Item *item)
{
	DrawTable *dt = item->header.dt_ptr;
	form_list_state *s = item->list.state;
	FlocaleWinString *str = dt->dt_Fstr;
	int height = item->list.row_height;
	int width = item->header.size_x;
	XRectangle clip = {BOX_SPC, BOX_SPC, width - 20,
		item->header.size_y - 2 * BOX_SPC};
	Region region = XCreateRegion();

	XClearWindow(dpy, item->header.win);
	XUnionRectWithRegion(&clip, region, region);
	str->flags.has_clip_region = True;
	str->clip_region = region;
	str->flags.has_colorset = False;
	str->win = item->header.win;
	str->gc = dt->dt_item_GC;
	for (int row = 0; row < s->rows && s->top + row < s->count; row++)
	{
		int index = s->top + row;
		int y = BOX_SPC + row * height;
		bool selected = s->bits[index / 8] & (1u << (index % 8));

		XSetForeground(dpy, dt->dt_item_GC,
			dt->dt_colors[selected ? c_item_fg : c_item_bg]);
		XFillRectangle(dpy, item->header.win, dt->dt_item_GC,
			BOX_SPC, y, width - 20, height);
		XSetForeground(dpy, dt->dt_item_GC,
			dt->dt_colors[selected ? c_item_bg : c_item_fg]);
		str->str = item->list.labels[index];
		str->len = strlen(str->str);
		str->x = BOX_SPC + TEXT_SPC;
		str->y = y + TEXT_SPC + dt->dt_Ffont->ascent;
		FlocaleDrawString(dpy, dt->dt_Ffont, str, FWS_HAVE_LENGTH);
		if (CF.cur_list == item && index == s->focus)
			XDrawRectangle(dpy, item->header.win, dt->dt_item_GC,
				BOX_SPC, y, width - 21, height - 1);
	}
	str->str = NULL;
	str->flags.has_clip_region = False;
	XDestroyRegion(region);
	XSetForeground(dpy, dt->dt_item_GC, dt->dt_colors[c_item_fg]);
	XDrawRectangle(dpy, item->header.win, dt->dt_item_GC,
		0, 0, width - 1, item->header.size_y - 1);
	/* A proportional track also accepts page clicks above/below its thumb. */
	if (s->count > s->rows)
	{
		int track = item->header.size_y - 2 * BOX_SPC;
		int thumb = track * s->rows / s->count;
		int y;

		if (thumb < 4)
			thumb = 4;
		y = BOX_SPC + (track - thumb) * s->top / (s->count - s->rows);
		XFillRectangle(dpy, item->header.win, dt->dt_item_GC,
			width - 14, y, 10, thumb);
	}
}

static void list_focus_item(Item *next)
{
	Item *old = CF.cur_list != NULL ? CF.cur_list : CF.cur_input;

	CF.cur_list = next->type == I_LIST ? next : NULL;
	CF.cur_input = next->type == I_INPUT ? next : NULL;
	CF.abs_cursor = CF.rel_cursor = 0;
	if (CF.cur_input != NULL)
		CF.cur_input->input.left = 0;
	RedrawItem(old, 1, NULL);
	RedrawItem(next, 0, NULL);
}

Bool form_list_key(KeySym key, unsigned int modifiers)
{
	Item *list = CF.cur_list;
	form_list_state *s;
	bool has_list = false;

	/* Keep the established input/history ring untouched in legacy forms. */
	if (key == XK_Tab || key == XK_ISO_Left_Tab)
	{
		Item *first = NULL, *last = NULL, *previous = NULL, *next = NULL;
		Item *current = list != NULL ? list : CF.cur_input;
		bool after = false;

		for (Item *p = root_item_ptr; p != NULL; p = p->header.next)
		{
			if (p->type == I_LIST)
				has_list = true;
			if (p->type != I_LIST && p->type != I_INPUT)
				continue;
			if (first == NULL)
				first = p;
			if (after && next == NULL)
				next = p;
			if (p == current)
			{
				previous = last;
				after = true;
			}
			last = p;
		}
		if (!has_list)
			return False;
		if ((modifiers & ShiftMask) || key == XK_ISO_Left_Tab)
			list_focus_item(previous != NULL ? previous : last);
		else
			list_focus_item(next != NULL ? next : first);
		return True;
	}
	if (list == NULL)
		return False;
	s = list->list.state;
	switch (key)
	{
	case XK_Up:
		form_list_focus(s, s->focus - 1);
		break;
	case XK_Down:
		form_list_focus(s, s->focus + 1);
		break;
	case XK_Home:
		form_list_focus(s, 0);
		break;
	case XK_End:
		form_list_focus(s, s->count - 1);
		break;
	case XK_Page_Up:
		form_list_focus(s, s->focus - s->rows);
		break;
	case XK_Page_Down:
		form_list_focus(s, s->focus + s->rows);
		break;
	case XK_space:
		form_list_toggle(s, s->focus);
		break;
	default:
		return False; /* F keys/Escape retain native button shortcuts. */
	}
	RedrawItem(list, 0, NULL);
	return True;
}

void form_list_click(Item *item, XButtonEvent *event)
{
	form_list_state *s = item->list.state;
	int row = (event->y - BOX_SPC) / item->list.row_height + s->top;

	if (event->button == Button4 || event->button == Button5)
		form_list_scroll(s, event->button == Button4 ? -3 : 3);
	else if (event->button == Button1)
	{
		list_focus_item(item);
		if (event->x >= item->header.size_x - 18 && s->count > s->rows)
		{
			int track = item->header.size_y - 2 * BOX_SPC;
			int thumb = track * s->rows / s->count;
			int top;

			if (thumb < 4)
				thumb = 4;
			top = BOX_SPC + (track - thumb) * s->top /
				(s->count - s->rows);
			if (event->y < top)
				form_list_scroll(s, -s->rows);
			else if (event->y >= top + thumb)
				form_list_scroll(s, s->rows);
		}
		else if (event->y >= BOX_SPC && row < s->count)
		{
			form_list_focus(s, row);
			form_list_toggle(s, row);
		}
	}
	RedrawItem(item, 0, NULL);
}
