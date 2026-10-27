/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include <stdio.h>
#include "libs/Module.h"
#include "libs/fvwmlib.h"
#include "libs/Colorset.h"
#include "FvwmForm.h"
#include "libs/flocale_font_types.h"

/* Native monochrome type marks scale with the item's font. They introduce
 * no paths, decoders, per-entry pixmaps or extra X resources. */
static void list_icon(Item *item, enum form_list_icon icon, int x, int y,
	int size)
{
	GC gc = item->header.dt_ptr->dt_item_GC;
	Window win = item->header.win;

	if (icon == FORM_ICON_DIRECTORY)
	{
		XDrawRectangle(dpy, win, gc, x, y + size / 4,
			size, 3 * size / 4);
		XDrawRectangle(dpy, win, gc, x, y, size / 2, size / 4);
	}
	else if (icon == FORM_ICON_LINK)
	{
		XDrawLine(dpy, win, gc, x, y + size, x + size, y);
		XDrawLine(dpy, win, gc, x + size / 3, y, x + size, y);
		XDrawLine(dpy, win, gc, x + size, y, x + size, y + 2 * size / 3);
	}
	else if (icon == FORM_ICON_FILE)
	{
		XDrawRectangle(dpy, win, gc, x + size / 8, y,
			3 * size / 4, size);
		XDrawLine(dpy, win, gc, x + size / 3, y + size / 3,
			x + 2 * size / 3, y + size / 3);
		XDrawLine(dpy, win, gc, x + size / 3, y + 2 * size / 3,
			x + 2 * size / 3, y + 2 * size / 3);
	}
	else
	{
		XDrawArc(dpy, win, gc, x, y, size, size, 0, 360 * 64);
	}
}

static void list_thumb(Item *item, int *top, int *height)
{
	form_list_state *s = item->list.state;
	int track = item->header.size_y - 2 * BOX_SPC;
	int lines = form_list_lines(s);

	*top = BOX_SPC;
	*height = track;
	if (lines <= s->rows)
		return;
	*height = track * s->rows / lines;
	if (*height < (CF.ui_metrics.thumb_min ? CF.ui_metrics.thumb_min : 4))
		*height = CF.ui_metrics.thumb_min ? CF.ui_metrics.thumb_min : 4;
	*top += (track - *height) * (s->top / form_list_columns(s)) /
		(lines - s->rows);
}

/* One child window; work is bounded by at most 32 rows x 8 columns. */
void form_list_draw(Item *item)
{
	DrawTable *dt = item->header.dt_ptr;
	form_list_state *s = item->list.state;
	FlocaleWinString *str = dt->dt_Fstr;
	int height = item->list.row_height;
	int width = item->header.size_x;
	int columns = form_list_columns(s);
	int cell_width = (width - 2 * BOX_SPC - LIST_GUTTER) / columns;

	XClearWindow(dpy, item->header.win);
	str->flags.has_colorset = False;
	str->win = item->header.win;
	str->gc = dt->dt_item_GC;
	for (int cell = 0; cell < s->rows * columns &&
	    s->top + cell < s->count; cell++)
	{
		int index = s->top + cell;
		int x = BOX_SPC + (cell % columns) * cell_width;
		int y = BOX_SPC + (cell / columns) * height;
		bool selected = s->bits[index / 8] & (1u << (index % 8));
		form_list_entry *entry = &item->list.entries[index];
		XRectangle clip = {x, y, cell_width, height};
		Region region = XCreateRegion();
		int icon_size = dt->dt_Ffont->height;

		XUnionRectWithRegion(&clip, region, region);
		str->flags.has_clip_region = True;
		str->clip_region = region;
		XSetForeground(dpy, dt->dt_item_GC,
			dt->dt_colors[selected ? c_item_fg : c_item_bg]);
		XFillRectangle(dpy, item->header.win, dt->dt_item_GC,
			x, y, cell_width, height);
		XSetForeground(dpy, dt->dt_item_GC,
			dt->dt_colors[selected ? c_item_bg : c_item_fg]);
		str->str = entry->label;
		str->len = strlen(str->str);
		str->x = x + TEXT_SPC;
		str->y = y + TEXT_SPC + dt->dt_Ffont->ascent;
		if (columns > 1 || item->list.viewport_columns > 1)
		{
			icon_size = height - 4 * TEXT_SPC - dt->dt_Ffont->height;
			if (icon_size < 8)
				icon_size = 8;
			if (icon_size > cell_width - 2 * TEXT_SPC)
				icon_size = cell_width - 2 * TEXT_SPC;
			XSetRegion(dpy, dt->dt_item_GC, region);
			list_icon(item, entry->icon,
				x + (cell_width - icon_size) / 2,
				y + TEXT_SPC, icon_size);
			XSetClipMask(dpy, dt->dt_item_GC, None);
			str->y = y + 2 * TEXT_SPC + icon_size +
				dt->dt_Ffont->ascent;
		}
		else if (entry->icon != FORM_ICON_NONE)
		{
			icon_size = height - 2 * TEXT_SPC;
			list_icon(item, entry->icon, x + TEXT_SPC,
				y + TEXT_SPC, icon_size);
			str->x += icon_size + TEXT_SPC;
		}
		FlocaleDrawString(dpy, dt->dt_Ffont, str, FWS_HAVE_LENGTH);
		if (CF.cur_list == item && index == s->focus)
		{
			int stroke = form_focus_stroke(CF.ui_metrics.stroke,
				cell_width, height);

			for (int inset = 0; inset < stroke; inset++)
				XDrawRectangle(dpy, item->header.win,
					dt->dt_item_GC, x + inset, y + inset,
					cell_width - 1 - 2 * inset,
					height - 1 - 2 * inset);
		}
		str->flags.has_clip_region = False;
		XDestroyRegion(region);
	}
	str->str = NULL;
	XSetForeground(dpy, dt->dt_item_GC, dt->dt_colors[c_item_fg]);
	XDrawRectangle(dpy, item->header.win, dt->dt_item_GC,
		0, 0, width - 1, item->header.size_y - 1);
	if (form_list_lines(s) > s->rows)
	{
		int top, thumb;

		list_thumb(item, &top, &thumb);
		XFillRectangle(dpy, item->header.win, dt->dt_item_GC,
			width - LIST_GUTTER +
			(CF.ui_metrics.thumb_inset ? CF.ui_metrics.thumb_inset : 2),
			top, CF.ui_metrics.thumb_width ? CF.ui_metrics.thumb_width : 10,
			thumb);
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

static void list_watch_change(Item *list, const form_list_state *before)
{
	char command[sizeof("Echo FVWM_FORM_CHANGED ") + 65];

	if (!form_list_watch_changed(&list->list.watch, before,
	    list->list.state))
		return;
	snprintf(command, sizeof(command), "Echo FVWM_FORM_CHANGED %s",
		list->list.watch.token);
	SendText(Channel, command, 0);
}

Bool form_list_key(KeySym key, unsigned int modifiers)
{
	Item *list = CF.cur_list;
	form_list_state *s;
	form_list_state before;
	int target, columns;
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
	before = *s;
	columns = form_list_columns(s);
	target = s->focus;
	switch (key)
	{
	case XK_Up:
		target -= columns;
		break;
	case XK_Down:
		target += columns;
		break;
	case XK_Left:
		if (columns == 1)
			return False;
		target--;
		break;
	case XK_Right:
		if (columns == 1)
			return False;
		target++;
		break;
	case XK_Home:
		target = 0;
		break;
	case XK_End:
		target = s->count - 1;
		break;
	case XK_Page_Up:
		target -= s->rows * columns;
		break;
	case XK_Page_Down:
		target += s->rows * columns;
		break;
	case XK_space:
		form_list_toggle(s, s->focus);
		list_watch_change(list, &before);
		RedrawItem(list, 0, NULL);
		return True;
	default:
		return False;
	}
	if (modifiers & ShiftMask)
		form_list_extend(s, target);
	else
		form_list_focus(s, target);
	list_watch_change(list, &before);
	RedrawItem(list, 0, NULL);
	return True;
}

void form_list_click(Item *item, XButtonEvent *event)
{
	form_list_state *s = item->list.state;
	form_list_state before = *s;
	int columns = form_list_columns(s);
	int cell_width = (item->header.size_x - 2 * BOX_SPC -
		LIST_GUTTER) / columns;
	int row = (event->y - BOX_SPC) / item->list.row_height;
	int column = (event->x - BOX_SPC) / cell_width;
	int index = s->top + row * columns + column;

	if (event->button == Button4 || event->button == Button5)
		form_list_scroll(s, event->button == Button4 ? -3 : 3);
	else if (event->button == Button1)
	{
		list_focus_item(item);
		if (event->x >= item->header.size_x - LIST_GUTTER -
		    (CF.ui_metrics.thumb_inset ? CF.ui_metrics.thumb_inset : 2))
		{
			int top, thumb;

			list_thumb(item, &top, &thumb);
			if (event->y < top)
				form_list_scroll(s, -s->rows);
			else if (event->y >= top + thumb)
				form_list_scroll(s, s->rows);
		}
		else if (event->y >= BOX_SPC && event->x >= BOX_SPC &&
		    row < s->rows && column < columns && index < s->count)
		{
			if (event->state & ShiftMask)
				form_list_extend(s, index);
			else
			{
				form_list_focus(s, index);
				form_list_toggle(s, index);
			}
		}
	}
	list_watch_change(item, &before);
	RedrawItem(item, 0, NULL);
}
