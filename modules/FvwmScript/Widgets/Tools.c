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
#include <limits.h>

#include "Tools.h"
#include "../script_delay.h"

#include "libs/FEvent.h"
#include "libs/log.h"
#include "libs/flocale_font_types.h"
/* helper functions */
void script_geometry_diagnostic(const char *operation, int id)
{
	if (log_get_fd() >= 0)
	{
		fvwm_debug(operation, "[%s]: widget %d has invalid geometry",
			ScriptName, id);
	}
	else
	{
		fprintf(stderr, "[%s]: %s: widget %d has invalid geometry\n",
			ScriptName, operation, id);
	}
}

bool script_widget_layout(struct XObj *xobj, FlocaleFont *font,
	script_layout_geometry *geometry)
{
	script_layout_input input = {0};
	char *title = NULL;
	char number[32];
	int maximum;

	/* Widget constants are the historic, consecutive 1..15 type table. */
	input.type = (script_layout_type)(xobj->TypeWidget - 1);
	input.width = xobj->width;
	input.height = xobj->height;
	input.x = xobj->x;
	input.y = xobj->y;
	input.icon_width = xobj->icon_w;
	input.icon_height = xobj->icon_h;
	input.has_icon = xobj->icon != NULL;
	input.has_title = xobj->title[0] != '\0';
	input.menu_x = 2;
	if (font != NULL)
	{
		input.font_height = font->height;
		/* Invalid metrics cannot safely be used by existing draw callbacks. */
		if (font->height < 0 || font->height > UINT16_MAX ||
		    font->ascent < 0 || font->ascent > UINT16_MAX ||
		    font->descent < 0 || font->descent > UINT16_MAX)
		{
			return false;
		}
		switch (xobj->TypeWidget)
		{
		case PushButton:
		case Menu:
			title = GetMenuTitle(xobj->title,
				xobj->TypeWidget == Menu ? 0 : 1);
			input.has_title = title[0] != '\0';
			input.text_width = FlocaleTextWidth(font, title, strlen(title));
			free(title);
			break;
		case HScrollBar:
		case VScrollBar:
			maximum = script_widget_initial_maximum(xobj->value2,
				xobj->value3, 10, true);
			snprintf(number, sizeof(number), "%d", xobj->value2);
			input.text_width = FlocaleTextWidth(font, number, strlen(number));
			snprintf(number, sizeof(number), "%d", maximum);
			input.second_text_width =
				FlocaleTextWidth(font, number, strlen(number));
			break;
		case PopupMenu:
			input.text_width = -4; /* No options retains legacy width 30. */
			maximum = CountOption(xobj->title);
			for (int i = 1; i <= maximum; i++)
			{
				int width;
				title = GetMenuTitle(xobj->title, i);
				width = FlocaleTextWidth(font, title, strlen(title));
				if (width > input.text_width)
				{
					input.text_width = width;
				}
				free(title);
			}
			break;
		default:
			input.text_width = FlocaleTextWidth(font, xobj->title,
				strlen(xobj->title));
			break;
		}
	}
	if (xobj->TypeWidget == Menu)
	{
		for (int i = 0; i < nbobj && tabxobj[i] != xobj; i++)
		{
			if (tabxobj[i] != NULL && tabxobj[i]->TypeWidget == Menu)
			{
				title = GetMenuTitle(tabxobj[i]->title, 0);
				input.menu_x += (int64_t)FlocaleTextWidth(
					tabxobj[i]->Ffont, title, strlen(title)) + 10;
				free(title);
			}
		}
	}
	return script_geometry_layout(&input, geometry);
}

bool script_widget_prepare(struct XObj *xobj)
{
	script_layout_geometry geometry;
	FlocaleFont *font = NULL;

	if (xobj->TypeWidget <= PopupMenu || xobj->TypeWidget >= List)
	{
		font = FlocaleLoadFont(dpy, xobj->font, ScriptName);
		if (font == NULL)
		{
			return false;
		}
	}
	if (!script_widget_layout(xobj, font, &geometry))
	{
		FlocaleUnloadFont(dpy, font);
		return false;
	}
	xobj->Ffont = font;
	xobj->width = geometry.width;
	xobj->height = geometry.height;
	xobj->x = geometry.x;
	xobj->y = geometry.y;
	return true;
}

bool script_widget_popup(struct XObj *xobj, int first, int padding,
	bool align_value, script_layout_geometry *geometry, int *row_height)
{
	int count = CountOption(xobj->title);
	int64_t row = (int64_t)xobj->Ffont->height + 10;
	int64_t height = ((int64_t)count - first + 1) * row;
	int64_t width = align_value ? xobj->width : 0;
	int64_t origin_y = align_value ? (int64_t)xobj->y -
		((int64_t)xobj->value - 1) * row :
		(int64_t)xobj->y + xobj->height;
	int x, y;
	Window child;

	*geometry = (script_layout_geometry){0};
	*row_height = 0;
	if (!script_geometry_size(1, height))
	{
		return false;
	}
	if (!align_value)
	{
		for (int i = first; i <= count; i++)
		{
			char *title = GetMenuTitle(xobj->title, i);
			int64_t measured = (int64_t)FlocaleTextWidth(xobj->Ffont,
				title, strlen(title)) + padding;
			if (measured > width)
			{
				width = measured;
			}
			free(title);
		}
	}
	if (!script_geometry_size(align_value ? width : width - 5, height) ||
	    !script_geometry_position(xobj->x, origin_y))
	{
		return false;
	}
	if (!XTranslateCoordinates(dpy, *xobj->ParentWin, Root, xobj->x,
	    (int)origin_y, &x, &y, &child))
	{
		return false;
	}
	/* Keep legacy placement for representable menus. Screen fitting is a
	 * separate policy; an oversized menu is refused rather than wrapped. */
	if (x < 0)
	{
		x = 0;
	}
	if (y < 0)
	{
		y = 0;
	}
	if ((int64_t)x + width > XDisplayWidth(dpy, screen))
	{
		x = (int)(XDisplayWidth(dpy, screen) - width);
	}
	if ((int64_t)y + height > XDisplayHeight(dpy, screen))
	{
		int64_t adjusted = align_value ?
			XDisplayHeight(dpy, screen) - height :
			(int64_t)y - height - xobj->height;
		if (!script_geometry_position(x, adjusted))
		{
			return false;
		}
		y = (int)adjusted;
	}
	if (!script_geometry_position(x, y))
	{
		return false;
	}
	geometry->x = x;
	geometry->y = y;
	geometry->width = (int)width;
	geometry->height = (int)height;
	*row_height = (int)row;
	return true;
}

/* get byte offset corresponding to character offset, including
   bounds check to represent end of text */
static int script_locale_stride(const char *text, void *context)
{
	return FlocaleStringNumberOfBytes(context, text);
}

size_t script_locale_text_next(FlocaleFont *flf, const char *str,
	size_t remaining)
{
	return script_text_next(str, remaining, script_locale_stride, flf);
}

int getByteOffsetBoundsCheck(FlocaleFont *flf, char *str, int offset)
{
	return script_text_byte_offset(str, strlen(str), offset,
		script_locale_stride, flf);
}

int getCharOffsetBoundsCheck(FlocaleFont *flf, char *str, int offset)
{
	return script_text_char_offset(str, strlen(str), offset,
		script_locale_stride, flf);
}

/*
 * Function for 3D writing
 */
void MyDrawString(
	Display *dpy, struct XObj *xobj, Window win, int x, int y,
	char *str, unsigned long ForeC,unsigned long HiC,
	unsigned long BackC, int WithRelief, XRectangle *clip, XEvent *evp)
{
	Region region = None;
	XRectangle inter;
	Bool do_draw = True;

	if (evp && clip)
	{
		if (!frect_get_intersection(
			    clip->x, clip->y, clip->width, clip->height,
			    evp->xexpose.x, evp->xexpose.y,
			    evp->xexpose.width, evp->xexpose.height,
			    &inter))
		{
			do_draw = False;
		}
	}
	else if (clip)
	{
		inter.x = clip->x;
		inter.y = clip->y;
		inter.width = clip->width;
		inter.height = clip->height;
	}
	else if (evp)
	{
		inter.x = evp->xexpose.x;
		inter.y = evp->xexpose.y;
		inter.width = evp->xexpose.width;
		inter.height = evp->xexpose.height;
	}
	if (!do_draw)
	{
		return;
	}
	if (clip || evp)
	{
		region = XCreateRegion();
		XUnionRectWithRegion (&inter, region, region);
		XSetClipRectangles(dpy, xobj->gc, 0, 0, &inter, 1, Unsorted);
		FwinString->flags.has_clip_region = True;
		FwinString->clip_region = region;
	}
	else
	{
		FwinString->flags.has_clip_region = False;
	}

	FwinString->win = win;
	FwinString->str = str;
	FwinString->gc = xobj->gc;
	FwinString->flags.has_colorset = False;
	if (xobj->colorset >= 0)
	{
		FwinString->colorset = GetColorset(xobj->colorset);
		FwinString->flags.has_colorset = True;
	}
	if (WithRelief && xobj->Ffont->shadow_size == 0)
	{
		XSetBackground(dpy, xobj->gc, xobj->TabColor[BackC]);
		XSetForeground(dpy, xobj->gc, xobj->TabColor[HiC]);
		FwinString->x = x+1;
		FwinString->y = y+1;
		FlocaleDrawString(dpy, xobj->Ffont, FwinString, 0);
		XSetForeground(dpy, xobj->gc, xobj->TabColor[ForeC]);
		FwinString->x = x;
		FwinString->y = y;
		FlocaleDrawString(dpy, xobj->Ffont, FwinString, 0);
	}
	else
	{
		XSetBackground(dpy, xobj->gc, xobj->TabColor[BackC]);
		XSetForeground(dpy, xobj->gc, xobj->TabColor[ForeC]);
		FwinString->x = x;
		FwinString->y = y+1;
		FlocaleDrawString(dpy, xobj->Ffont, FwinString, 0);
	}
	if (region)
	{
		XDestroyRegion(region);
		XSetClipMask(dpy, xobj->gc, None);
	}
	FwinString->flags.has_clip_region = False;
}

/*
 * Return the x text position of the widget
 */
int GetXTextPosition(struct XObj *xobj, int obj_width, int str_len,
		     int left_offset, int center_offset, int right_offset)
{
	int position;
	int x;

	if (!IS_TEXT_POS_DEFAULT(xobj))
	{
		position = GET_TEXT_POS(xobj);
	}
	else
	{
		switch (xobj->TypeWidget)
		{
		case ItemDraw:
		case PushButton:
			position = TEXT_POS_CENTER;
			break;
		default:
			position = TEXT_POS_LEFT;
			break;
		}
	}

	if (position == TEXT_POS_CENTER)
	{
		x = (obj_width - str_len)/2 + center_offset;
	}
	else if (position == TEXT_POS_LEFT)
	{
		x = left_offset;
	}
	else /* position == TEXT_POS_RIGHT */
	{
		x = (obj_width - str_len) - right_offset;
	}
	return x;
}

/*
 * Returns menu's option id title
 */
char *GetMenuTitle(char *str, int id)
{
	size_t length;
	const char *option = script_text_option(str, id, &length);
	char *title = fxmalloc(length + 1);

	memcpy(title, option, length);
	title[length] = '\0';
	return title;
}

/*
 * Draw popup menu window content
 */
void DrawPMenu(struct XObj *xobj, Window WinPop, int h, int StrtOpt)
{
	XSegment segm[2];
	unsigned int i;
	int x,y;
	unsigned int width,height;
	Window Root;

	if (!XGetGeometry(dpy, WinPop, &Root, &x, &y, &width, &height, &i, &i))
		return;
	for (i=0; i<2; i++)
	{
		segm[0].x1 = i;
		segm[0].y1 = i;
		segm[0].x2 = width-i-1;
		segm[0].y2 = i;

		segm[1].x1 = i;
		segm[1].y1 = i;
		segm[1].x2 = i;
		segm[1].y2 = height-i-1;
		XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
		XDrawSegments(dpy, WinPop, xobj->gc, segm, 2);

		segm[0].x1 = 1+i;
		segm[0].y1 = height-i-1;
		segm[0].x2 = width-i-1;
		segm[0].y2 = height-i-1;

		segm[1].x1 = width-i-1;
		segm[1].y1 = i;
		segm[1].x2 = width-i-1;
		segm[1].y2 = height-i-1;
		XSetForeground(dpy, xobj->gc, xobj->TabColor[shad]);
		XDrawSegments(dpy, WinPop, xobj->gc, segm, 2);
	}
	/* Write options */
	for (i=StrtOpt; i <= xobj->value3; i++)
		UnselectMenu(
			xobj, WinPop, h, i, width, xobj->Ffont->ascent, StrtOpt);
}

void UnselectMenu(struct XObj *xobj, Window WinPop, int hOpt, int value,
		  unsigned int width, int asc, int start)
{
	int y,x,len;
	char *str;

	y = hOpt * (value - 1);
	XClearArea(dpy, WinPop, 2, y + 2, width - 4, hOpt - 4, False);
	str = (char*)GetMenuTitle(xobj->title, value + start);
	y += asc + 4;
	len = strlen(str);
	x = GetXTextPosition(xobj, width, FlocaleTextWidth(xobj->Ffont,str,len),
			     8, 0, 8);
	MyDrawString(dpy, xobj, WinPop, x, y, str, fore, hili, back,
		     !xobj->flags[1], NULL, NULL);
	free(str);
}

/*
 * Draw menu's active option
 */
void SelectMenu(struct XObj *xobj, Window WinPop, int hOpt, int value)
{
	XSegment segm[2];
	unsigned int i;
	int x,y;
	unsigned int width,height;
	Window Root;

	if (!XGetGeometry(dpy, WinPop, &Root, &x, &y, &width, &height, &i, &i))
		return;
	y = hOpt*(value-1);
	for (i=0; i<2; i++)
	{
		segm[0].x1 = i+2;
		segm[0].y1 = i+y+2;
		segm[0].x2 = width-i-3;
		segm[0].y2 = i+y+2;

		segm[1].x1 = i+2;
		segm[1].y1 = i+y+2;
		segm[1].x2 = i+2;
		segm[1].y2 = y+hOpt-4-i;
		XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
		XDrawSegments(dpy, WinPop, xobj->gc, segm, 2);

		segm[0].x1 = i+3;
		segm[0].y1 = y-i-3+hOpt;
		segm[0].x2 = width-i-3;
		segm[0].y2 = y-i-3+hOpt;

		segm[1].x1 = width-i-3;
		segm[1].y1 = i+y+2;
		segm[1].x2 = width-i-3;
		segm[1].y2 = i+y-4+hOpt;
		XSetForeground(dpy, xobj->gc, xobj->TabColor[shad]);
		XDrawSegments(dpy, WinPop, xobj->gc, segm, 2);
	}
}

/*
 * Get the number of options in a menu
 */
int CountOption(char *str)
{
	int i=1;
	int w=0;

	while (str[w] != '\0')
	{
		if (str[w] == '|' && i < INT_MAX) i++;
		w++;
	}

	return i;
}


/*
 * Draw widget icon and title
 */
void DrawIconStr(
	int offset, struct XObj *xobj, int DoRedraw,
	int l_offset, int c_offset, int r_offset,
	XRectangle *str_clip, XRectangle *icon_clip, XEvent *evp)
{
	int i,j;
	char *str;
	int len = 0;
	XRectangle clear_r, inter;
	Bool do_clear = True;

	inter.x = clear_r.x = 4;
	inter.y = clear_r.y = 4;
	inter.width = clear_r.width  = xobj->width-8;
	inter.height = clear_r.height = xobj->height-8;

	if (evp)
	{
		if (!frect_get_intersection(
			    clear_r.x, clear_r.y, clear_r.width,
			    clear_r.height, evp->xexpose.x, evp->xexpose.y,
			    evp->xexpose.width, evp->xexpose.height,
			    &inter))
		{
			do_clear = False;
		}
	}
#if 0
	else if (clip)
	{
		inter.x = clip->x;
		inter.y = clip->y;
		inter.width = clip->width;
		inter.height = clip->height;
	}
	else if (evp)
	{
		inter.x = evp->xexpose.x;
		inter.y = evp->xexpose.y;
		inter.width = evp->xexpose.width;
		inter.height = evp->xexpose.height;
	}
#endif
	if (DoRedraw && do_clear)
	{
		XClearArea(
			dpy, xobj->win,
			inter.x, inter.y, inter.width, inter.height, False);
	}

	str = GetMenuTitle(xobj->title,1);
	len = strlen(str);
	i = GetXTextPosition(
		xobj, xobj->width, FlocaleTextWidth(xobj->Ffont,str,len),
		l_offset, c_offset, r_offset);

	if (len > 0 && xobj->iconPixmap==None)
	{
		/* if no icon */
		j = xobj->height/2 - (xobj->Ffont->height)/2 +
			xobj->Ffont->ascent + offset;
		MyDrawString(
			dpy,xobj,xobj->win,i,j,str,fore,hili,back,
			!xobj->flags[1], str_clip, evp);
	}
	else
	{
		/* if icon exists */
		FvwmRenderAttributes fra;
		Bool do_draw_icon = True;
		XRectangle ir;
		int iy = (xobj->height - xobj->icon_h)/2+offset;
		int ix = (xobj->width - xobj->icon_w)/2+offset;

		if (evp && icon_clip)
		{
			if (!frect_get_intersection(
				    icon_clip->x, icon_clip->y,
				    icon_clip->width, icon_clip->height,
				    evp->xexpose.x, evp->xexpose.y,
				    evp->xexpose.width, evp->xexpose.height,
				    &inter))
			{
				do_draw_icon = False;
			}
			else if (!frect_get_intersection(
					 inter.x, inter.y,
					 inter.width, inter.height,
					 ix, iy, xobj->icon_w, xobj->icon_h,
					 &ir))
			{
				do_draw_icon = False;
			}
		}
		else if (icon_clip)
		{
			if (!frect_get_intersection(
				    icon_clip->x, icon_clip->y,
				    icon_clip->width, icon_clip->height,
				    ix, iy, xobj->icon_w, xobj->icon_h,
				    &ir))
			{
				do_draw_icon = False;
			}
		}
		else if (evp)
		{
			if (!frect_get_intersection(
				    evp->xexpose.x, evp->xexpose.y,
				    evp->xexpose.width, evp->xexpose.height,
				    ix, iy, xobj->icon_w, xobj->icon_h,
				    &ir))
			{
				do_draw_icon = False;
			}
		}
		else
		{
			ir.x = ix;
			ir.y = iy;
			ir.width = xobj->icon_w;
			ir.height = xobj->icon_h;
		}

		if (len > 0)
		{
			j = ((xobj->height - xobj->icon_h)/4)*3 +
				xobj->icon_h + offset + xobj->Ffont->ascent;
			MyDrawString(
				dpy,xobj,xobj->win,i,j,str,fore,hili,
				back,!xobj->flags[1], str_clip, evp);
		}
		/* Draw icon */
		fra.mask = FRAM_DEST_IS_A_WINDOW;
		if (xobj->colorset >= 0)
		{
			fra.mask |= FRAM_HAVE_ICON_CSET;
			fra.colorset = GetColorset(xobj->colorset);
		}
		if (do_draw_icon)
		{
			PGraphicsRenderPixmaps(
				dpy, xobj->win, xobj->iconPixmap,
				xobj->icon_maskPixmap, xobj->icon_alphaPixmap,
				Pdepth, &fra, xobj->win, xobj->gc, None, None,
				ir.x - ix, ir.y - iy, ir.width, ir.height,
				ir.x, ir.y, ir.width, ir.height, False);
		}
	}
	free(str);
}

/*
 * Function to draw a 3D rectangle
 */
void DrawReliefRect(int x, int y, int width, int height, struct XObj *xobj,
		    unsigned int LiC, unsigned int ShadC)
{
	XSegment segm[2];
	int i;
	int j;

	width--;
	height--;

	for (i=0; i<2; i++)
	{
		j = -1-i;
		segm[0].x1 = i+x;
		segm[0].y1 = i+y;
		segm[0].x2 = i+x;
		segm[0].y2 = height+j+y+1;
		segm[1].x1 = i+x;
		segm[1].y1 = i+y;
		segm[1].x2 = width+j+x+1;
		segm[1].y2 = i+y;
		XSetForeground(dpy, xobj->gc, xobj->TabColor[LiC]);
		XDrawSegments(dpy, xobj->win, xobj->gc, segm, 2);

		segm[0].x1 = width+j+x+1;
		segm[0].y1 = i+1+y;
		segm[0].x2 = width+j+x+1;
		segm[0].y2 = height+j+y+1;
		segm[1].x1 = i+1+x;
		segm[1].y1 = height+j+y+1;
		segm[1].x2 = width+j+x+1;
		segm[1].y2 = height+j+y+1;
		XSetForeground(dpy, xobj->gc, xobj->TabColor[ShadC]);
		XDrawSegments(dpy, xobj->win, xobj->gc, segm, 2);
	}
	XSetForeground(dpy, xobj->gc, xobj->TabColor[fore]);
}

/* Read the bounded byte interval between two character positions. */
char *GetText(struct XObj *xobj, int End)
{
	int first = getByteOffsetBoundsCheck(xobj->Ffont, xobj->title,
		xobj->value);
	int last = getByteOffsetBoundsCheck(xobj->Ffont, xobj->title, End);
	char *text;
	size_t length;

	if (first > last)
	{
		int swap = first;
		first = last;
		last = swap;
	}
	length = (size_t)(last - first);
	text = fxmalloc(length + 1);
	memcpy(text, xobj->title + first, length);
	text[length] = '\0';
	return text;
}

void UnselectAllTextField(struct XObj **txobj)
{
	int i;

	for (i=0; i<nbobj; i++)
	{
		if (txobj[i]->TypeWidget == TextField)
		{
			if (txobj[i]->value2 != txobj[i]->value)
			{
				txobj[i]->value2 = txobj[i]->value;
				txobj[i]->DrawObj(txobj[i],NULL);
				return;
			}
		}
	}
}

void SelectOneTextField(struct XObj *xobj)
{
	int i;

	for (i=0; i<nbobj; i++)
	{
		if ((tabxobj[i]->TypeWidget == TextField) &&
		    (xobj != tabxobj[i]))
		{
			if (tabxobj[i]->value2 != tabxobj[i]->value)
			{
				tabxobj[i]->value2 = tabxobj[i]->value;
				tabxobj[i]->DrawObj(tabxobj[i],NULL);
				return;
			}
		}
	}
}

/*
 * Draw a North direction arrow
 */
void DrawArrowN(struct XObj *xobj, int x, int y, int Press)
{
	XSegment segm[4];

	segm[0].x1 = 5+x;
	segm[0].y1 = 1+y;
	segm[0].x2 = 0+x;
	segm[0].y2 = 12+y;
	segm[1].x1 = 5+x;
	segm[1].y1 = 3+y;
	segm[1].x2 = 1+x;
	segm[1].y2 = 12+y;
	if (Press == 1)
		XSetForeground(dpy, xobj->gc, xobj->TabColor[shad]);
	else
		XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
	XDrawSegments(dpy, xobj->win, xobj->gc, segm, 2);

	segm[0].x1 = 2+x;
	segm[0].y1 = 11+y;
	segm[0].x2 = 11+x;
	segm[0].y2 = 11+y;
	segm[1].x1 = 1+x;
	segm[1].y1 = 12+y;
	segm[1].x2 = 12+x;
	segm[1].y2 = 12+y;

	segm[2].x1 = 6+x;
	segm[2].y1 = 0+y;
	segm[2].x2 = 12+x;
	segm[2].y2 = 12+y;
	segm[3].x1 = 6+x;
	segm[3].y1 = 2+y;
	segm[3].x2 = 10+x;
	segm[3].y2 = 11+y;
	if (Press == 1)
		XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
	else
		XSetForeground(dpy, xobj->gc, xobj->TabColor[shad]);
	XDrawSegments(dpy, xobj->win, xobj->gc, segm, 4);
}

/*
 * Draw a South direction arrow
 */
void DrawArrowS(struct XObj *xobj, int x, int y, int Press)
{
	XSegment segm[4];

	segm[0].x1 = 0+x;
	segm[0].y1 = 0+y;
	segm[0].x2 = 12+x;
	segm[0].y2 = 0+y;
	segm[1].x1 = 1+x;
	segm[1].y1 = 1+y;
	segm[1].x2 = 11+x;
	segm[1].y2 = 1+y;

	segm[2].x1 = 1+x;
	segm[2].y1 = 1+y;
	segm[2].x2 = 5+x;
	segm[2].y2 = 10+y;
	segm[3].x1 = 2+x;
	segm[3].y1 = 1+y;
	segm[3].x2 = 5+x;
	segm[3].y2 = 8+y;
	if (Press == 1)
		XSetForeground(dpy, xobj->gc, xobj->TabColor[shad]);
	else
		XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
	XDrawSegments(dpy, xobj->win, xobj->gc, segm, 4);

	segm[0].x1 = 6+x;
	segm[0].y1 = 11+y;
	segm[0].x2 = 12+x;
	segm[0].y2 = 1+y;
	segm[1].x1 = 6+x;
	segm[1].y1 = 10+y;
	segm[1].x2 = 10+x;
	segm[1].y2 = 2+y;
	if (Press == 1)
		XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
	else
		XSetForeground(dpy, xobj->gc, xobj->TabColor[shad]);
	XDrawSegments(dpy, xobj->win, xobj->gc, segm, 2);
}

void DrawArrowE(struct XObj *xobj, int x, int y, int Press)
{
	XSegment segm[4];

	segm[0].x1 = 12+x;
	segm[0].y1 = 6+y;
	segm[0].x2 = 1+x;
	segm[0].y2 = 12+y;
	segm[1].x1 = 10+x;
	segm[1].y1 = 6+y;
	segm[1].x2 = 2+x;
	segm[1].y2 = 10+y;
	if (Press == 1)
		XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
	else
		XSetForeground(dpy, xobj->gc, xobj->TabColor[shad]);
	XDrawSegments(dpy, xobj->win, xobj->gc, segm, 2);

	segm[0].x1 = 0+x;
	segm[0].y1 = 0+y;
	segm[0].x2 = 0+x;
	segm[0].y2 = 12+y;
	segm[1].x1 = 1+x;
	segm[1].y1 = 0+y;
	segm[1].x2 = 1+x;
	segm[1].y2 = 11+y;

	segm[2].x1 = 0+x;
	segm[2].y1 = 0+y;
	segm[2].x2 = 11+x;
	segm[2].y2 = 5+y;
	segm[3].x1 = 0+x;
	segm[3].y1 = 1+y;
	segm[3].x2 = 9+x;
	segm[3].y2 = 5+y;
	if (Press == 1)
		XSetForeground(dpy, xobj->gc, xobj->TabColor[shad]);
	else
		XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
	XDrawSegments(dpy, xobj->win, xobj->gc, segm, 4);
}

void DrawArrowW(struct XObj *xobj, int x, int y, int Press)
{
	XSegment segm[4];

	segm[0].x1 = 2+x;
	segm[0].y1 = 5+y;
	segm[0].x2 = 12+x;
	segm[0].y2 = 0+y;
	segm[1].x1 = 4+x;
	segm[1].y1 = 5+y;
	segm[1].x2 = 10+x;
	segm[1].y2 = 2+y;
	if (Press == 1)
		XSetForeground(dpy, xobj->gc, xobj->TabColor[shad]);
	else
		XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
	XDrawSegments(dpy, xobj->win, xobj->gc, segm, 2);

	segm[0].x1 = 0+x;
	segm[0].y1 = 6+y;
	segm[0].x2 = 12+x;
	segm[0].y2 = 12+y;
	segm[1].x1 = 2+x;
	segm[1].y1 = 6+y;
	segm[1].x2 = 11+x;
	segm[1].y2 = 10+y;

	segm[2].x1 = 12+x;
	segm[2].y1 = 1+y;
	segm[2].x2 = 12+x;
	segm[2].y2 = 12+y;
	segm[3].x1 = 11+x;
	segm[3].y1 = 2+y;
	segm[3].x2 = 11+x;
	segm[3].y2 = 11+y;
	if (Press == 1)
		XSetForeground(dpy, xobj->gc, xobj->TabColor[hili]);
	else
		XSetForeground(dpy, xobj->gc, xobj->TabColor[shad]);
	XDrawSegments(dpy, xobj->win, xobj->gc, segm, 4);
}

int PtInRect(XPoint pt, XRectangle rect)
{
	return ((pt.x >= rect.x) && (pt.y >= rect.y) &&
		(pt.x <= rect.x+rect.width) && (pt.y <= rect.y+rect.height));
}

/* Modal callers retain the object, but not an old font/title snapshot. */
bool script_widget_next_event(struct XObj *object, XEvent *event)
{
	while (FvwmScriptModalEvent(object, event, 100))
	{
		if (event->type == KeyPress &&
		    fvwm_KeycodeToKeysym(dpy, event->xkey.keycode, 0, 0) == XK_Escape)
			return false;
		if (event->type != 0)
			return true;
	}
	return false;
}

bool script_widget_wait_release(struct XObj *object, int ticks)
{
	struct timespec start, now, duration;
	XEvent event;

	script_delay_duration(ticks, &duration);
	if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
		return false;
	for (;;)
	{
		/* A held repeat uses the original deadline; motion does not reset it.
		 * Consume queued X events so unrelated input cannot cause a spin. */
		if (!FvwmScriptModalEvent(object, &event, 20))
			return false;
		if (event.type == ButtonRelease)
			return false;
		if (event.type == KeyPress &&
		    fvwm_KeycodeToKeysym(dpy, event.xkey.keycode, 0, 0) == XK_Escape)
			return false;
		if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
			return false;
		if (now.tv_nsec < start.tv_nsec)
		{
			now.tv_sec--;
			now.tv_nsec += 1000000000L;
		}
		if (now.tv_sec - start.tv_sec > duration.tv_sec ||
		    (now.tv_sec - start.tv_sec == duration.tv_sec &&
		     now.tv_nsec - start.tv_nsec >= duration.tv_nsec))
			return true;
	}
}
