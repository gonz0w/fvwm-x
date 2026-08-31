/* -*-c-*- */
/* Copyright (C) 2001  Olivier Chapuis */
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

#include <stdio.h>
#include <stdint.h>

#include "libs/fvwmlib.h"
#include "libs/FShape.h"
#include "libs/PictureBase.h"
#include "libs/Picture.h"
#include "libs/PictureUtils.h"
#include "libs/PictureImageLoader.h"
#include "libs/FRenderInit.h"
#include "libs/Graphics.h"
#include "libs/ImageSafety.h"
#include "libs/Strings.h"
#include "libs/log.h"
#include "libs/wm_ui_metrics.h"
#include "libs/FEvent.h"
#include "fvwm.h"
#include "externs.h"
#include "window_flags.h"
#include "cursor.h"
#include "functions.h"
#include "misc.h"
#include "screen.h"
#include "module_interface.h"
#include "borders.h"
#include "icons.h"
#include "ewmh.h"
#include "ewmh_intern.h"
#include "geometry.h"

/*
 * net icon handler
 */
int ewmh_WMIcon(
	FvwmWindow *fw, XEvent *ev, window_style *style, unsigned long any)
{
	CARD32 *list = NULL;
	CARD32 *new_list = NULL;
	CARD32 *dummy = NULL;
	int size = 0;

	if (ev != NULL && HAS_EWMH_WM_ICON_HINT(fw) == EWMH_FVWM_ICON)
	{
		/* this event has been produced by fvwm itself */
		return 0;
	}

	list = ewmh_AtomGetByName(FW_W(fw),"_NET_WM_ICON",
				  EWMH_ATOM_LIST_PROPERTY_NOTIFY, &size);

	if (list != NULL && HAS_EWMH_WM_ICON_HINT(fw) == EWMH_NO_ICON)
	{
		/* the application have a true _NET_WM_ICON */
		SET_HAS_EWMH_WM_ICON_HINT(fw, EWMH_TRUE_ICON);
	}

	if (list == NULL || HAS_EWMH_WM_ICON_HINT(fw) != EWMH_TRUE_ICON)
	{
		/* No net icon or we have set the net icon */
		if (DO_EWMH_DONATE_ICON(fw) &&
		    (new_list =
		     ewmh_SetWmIconFromPixmap(
			     fw, list, &size, False)) != NULL)
		{
			SET_HAS_EWMH_WM_ICON_HINT(fw, EWMH_FVWM_ICON);
		}
	}
	else if (ev != NULL && USE_EWMH_ICON(fw))
	{
		/* client message. the application change its net icon */
		ChangeIconPixmap(fw);
	}
	if (FMiniIconsSupported)
	{
		if (list == NULL ||
		    HAS_EWMH_WM_ICON_HINT(fw) != EWMH_TRUE_ICON)
		{
			/* No net icon or we have set the net icon */
			if (DO_EWMH_DONATE_MINI_ICON(fw) &&
			    (dummy = ewmh_SetWmIconFromPixmap(
				    fw, (new_list != NULL)? new_list : list,
				    &size, True)) != NULL)
			{
				SET_HAS_EWMH_WM_ICON_HINT(
					fw, EWMH_FVWM_ICON);
				free(dummy);
			}
		}
		else
		{
			/* the application has a true ewmh icon */
			if (EWMH_SetIconFromWMIcon(fw, list, size, True))
			{
				SET_HAS_EWMH_MINI_ICON(fw, True);
			}
		}
	}

	if (list != NULL)
	{
		free(list);
	}
	if (new_list != NULL)
	{
		free(new_list);
	}
	return 0;
}

/*
 * update
 */
void EWMH_DoUpdateWmIcon(FvwmWindow *fw, Bool mini_icon, Bool icon)
{
	CARD32 *list = NULL;
	CARD32 *new_list = NULL;
	CARD32 *dummy = NULL;
	int size = 0;
	Bool icon_too = False;

	if (HAS_EWMH_WM_ICON_HINT(fw) == EWMH_TRUE_ICON)
	{
		return;
	}

	/* first see if we have to delete */
	if (FMiniIconsSupported && mini_icon &&
	    !DO_EWMH_DONATE_MINI_ICON(fw))
	{
		if (icon && !DO_EWMH_DONATE_ICON(fw))
		{
			icon_too = True;
		}
		EWMH_DeleteWmIcon(fw, True, icon_too);
	}
	if (!icon_too && icon && !DO_EWMH_DONATE_ICON(fw))
	{
		EWMH_DeleteWmIcon(fw, False, True);
	}

	/* now set if needed */
	if ((mini_icon && DO_EWMH_DONATE_MINI_ICON(fw)) ||
	    (icon && DO_EWMH_DONATE_ICON(fw)))
	{
		list = ewmh_AtomGetByName(
			FW_W(fw),"_NET_WM_ICON",
			EWMH_ATOM_LIST_PROPERTY_NOTIFY, &size);
	}
	else
	{
		return;
	}

	/* we have to reset */
	if (icon && DO_EWMH_DONATE_ICON(fw))
	{
		if ((new_list = ewmh_SetWmIconFromPixmap(
			     fw, list, &size, False)) != NULL)
		{
			SET_HAS_EWMH_WM_ICON_HINT(fw, EWMH_FVWM_ICON);
		}
	}
	if (FMiniIconsSupported && mini_icon && DO_EWMH_DONATE_MINI_ICON(fw))
	{
		if ((dummy = ewmh_SetWmIconFromPixmap(
			     fw, (new_list != NULL)? new_list : list, &size,
			     True)) != NULL)
		{
			SET_HAS_EWMH_WM_ICON_HINT(fw, EWMH_FVWM_ICON);
			free(dummy);
		}
	}
	if (list != NULL)
	{
		free(list);
	}
	if (new_list != NULL)
	{
		free(new_list);
	}
}

/*
 * build and set a net icon from a pixmap
 */
CARD32 *ewmh_SetWmIconFromPixmap(
	FvwmWindow *fw, CARD32 *orig_icon, int *orig_size, Bool is_mini_icon)
{
	CARD32 *new_icon = NULL;
	int keep_start = 0, keep_length = 0;
	int width = 0, height = 0;
	int i,j,k,l,m;
	int s;
	Pixmap pixmap = None;
	Pixmap mask = None;
	Pixmap alpha = None;
	XImage *image;
	XImage *m_image = NULL;
	XImage *a_image = NULL;
	int save_picture_w_g_width = 0;
	int save_picture_w_g_height = 0;
	int save_icon_depth = 0;
	Pixmap save_icon_pixmap = None;
	Pixmap save_icon_mask = None;
	Pixmap save_icon_alpha = None;
	int save_icon_nalloc_pixels = 0;
	Pixel *save_icon_alloc_pixels = NULL;
	int save_icon_no_limit = 0;
	Window save_icon_pixmap_w = None;
	Bool is_pixmap_ours = False;
	Bool is_icon_ours = False;
	Bool is_icon_shaped = False;
	Bool destroy_icon_pix = False;

	s = *orig_size / sizeof(CARD32);
	*orig_size = 0;

	if (is_mini_icon)
	{
		if (FMiniIconsSupported && fw->mini_icon != NULL)
		{
			pixmap = fw->mini_icon->picture;
			mask = fw->mini_icon->mask;
			alpha = fw->mini_icon->alpha;
			width = fw->mini_icon->width;
			height = fw->mini_icon->height;
		}
	}
	else
	{
		/* should save and restore any iformation modified by
		 * a call to GetIconPicture */
		save_picture_w_g_width = fw->icon_g.picture_w_g.width;
		save_picture_w_g_height = fw->icon_g.picture_w_g.height;
		save_icon_depth = fw->iconDepth;
		save_icon_pixmap = fw->iconPixmap;
		save_icon_mask = fw->icon_maskPixmap;
		save_icon_alpha = fw->icon_alphaPixmap;
		save_icon_nalloc_pixels = fw->icon_nalloc_pixels;
		save_icon_alloc_pixels = fw->icon_alloc_pixels;
		save_icon_no_limit = fw->icon_no_limit;
		save_icon_pixmap_w =  FW_W_ICON_PIXMAP(fw);
		is_pixmap_ours = IS_PIXMAP_OURS(fw);
		is_icon_ours = IS_ICON_OURS(fw);
		is_icon_shaped = IS_ICON_SHAPED(fw);
		GetIconPicture(fw, True);
		if (IS_PIXMAP_OURS(fw))
		{
			destroy_icon_pix = True;
		}
		pixmap = fw->iconPixmap;
		mask = fw->icon_maskPixmap;
		alpha = fw->icon_alphaPixmap;
		width = fw->icon_g.picture_w_g.width;
		height = fw->icon_g.picture_w_g.height;
		if (fw->icon_alloc_pixels != NULL)
		{
			if (fw->icon_nalloc_pixels != 0)
			{
				PictureFreeColors(
					dpy, Pcmap, fw->icon_alloc_pixels,
					fw->icon_nalloc_pixels, 0,
					fw->icon_no_limit);
			}
			free(fw->icon_alloc_pixels);
		}

		fw->icon_g.picture_w_g.width = save_picture_w_g_width;
		fw->icon_g.picture_w_g.height = save_picture_w_g_height;
		fw->iconDepth = save_icon_depth;
		fw->iconPixmap = save_icon_pixmap;
		fw->icon_maskPixmap = save_icon_mask;
		fw->icon_alphaPixmap = save_icon_alpha;
		fw->icon_nalloc_pixels = save_icon_nalloc_pixels;
		fw->icon_alloc_pixels = save_icon_alloc_pixels;
		fw->icon_no_limit = save_icon_no_limit;
		FW_W_ICON_PIXMAP(fw) = save_icon_pixmap_w;
		SET_ICON_OURS(fw, is_icon_ours);
		SET_PIXMAP_OURS(fw, is_pixmap_ours);
		SET_ICON_SHAPED(fw, is_icon_shaped);
	}

	if (pixmap == None)
	{
		return NULL;
	}

	if (FMiniIconsSupported && orig_icon != NULL)
	{
		int k_width = (is_mini_icon)?
			fw->ewmh_icon_width : fw->ewmh_mini_icon_width;
		int k_height = (is_mini_icon)?
			fw->ewmh_icon_height : fw->ewmh_mini_icon_height;

		for (i = 0; i < s - 1 && i >= 0; )
		{
			if (i + 1 + orig_icon[i]*orig_icon[i+1] < s)
			{
				if (orig_icon[i] == k_width &&
				    orig_icon[i+1] == k_height)
				{
					keep_start = i;
					keep_length = 2 +
						orig_icon[i] * orig_icon[i+1];
					i = s;
				}
			}
			if (i != s && orig_icon[i]*orig_icon[i+1] > 0)
			{
				i = i + 2 + orig_icon[i]*orig_icon[i+1];
			}
			else
			{
				i = s;
			}
		}
	}

	image = XGetImage(
		dpy, pixmap, 0, 0, width, height, AllPlanes, ZPixmap);
	if (image == NULL)
	{
		fvwm_debug(__func__,
			   "cannot create XImage\n");
		if (destroy_icon_pix)
		{
			XFreePixmap(dpy, pixmap);
			if (mask != None)
			{
				XFreePixmap(dpy, mask);
			}
			if (alpha != None)
			{
				XFreePixmap(dpy, alpha);
			}
		}
		return NULL;
	}

	if (mask != None)
	{
		m_image = XGetImage(dpy, mask, 0, 0, width, height,
				    AllPlanes, ZPixmap);
	}
	if (alpha != None)
	{
		a_image = XGetImage(dpy, alpha, 0, 0, width, height,
				    AllPlanes, ZPixmap);
	}
	*orig_size = (height*width + 2 + keep_length) * sizeof(CARD32);
	new_icon = fxmalloc(*orig_size);
	if (keep_length > 0)
	{
		memcpy(new_icon, &orig_icon[keep_start],
		       keep_length * sizeof(CARD32));
	}
	new_icon[keep_length] = width;
	new_icon[1+keep_length] = height;

	k = 0;
	l = (2 + keep_length);
	m = 0;

	switch(image->depth)
	{
	case 1:
	{
		XColor colors[2];
		CARD32 fg, bg;

		colors[0].pixel = fw->colors.fore;
		colors[1].pixel = fw->colors.back;
		XQueryColors(dpy, Pcmap, colors, 2);
		fg = 0xff000000 + (((colors[0].red >> 8) & 0xff) << 16) +
			(((colors[0].green >> 8) & 0xff) << 8) +
			((colors[0].blue >> 8) & 0xff);
		bg = 0xff000000 + (((colors[1].red >> 8) & 0xff) << 16) +
			(((colors[1].green >> 8) & 0xff) << 8) +
			((colors[1].blue >> 8) & 0xff);
		for (j = 0; j < height; j++)
		{
			for (i = 0; i < width; i++)
			{
				if (m_image != NULL &&
				    (XGetPixel(m_image, i, j) == 0))
				{
					new_icon[l++] = 0;
				}
				else if (XGetPixel(image, i, j) == 0)
				{
					new_icon[l++] = bg;
				}
				else
				{
					new_icon[l++] = fg;
				}
			}
		}
		break;

	}
	default: /* depth = Pdepth */
	{
		unsigned char *cm;
		XColor *colors;

		colors = fxmalloc(width * height * sizeof(XColor));
		cm = fxmalloc(width * height * sizeof(char));
		for (j = 0; j < height; j++)
		{
			for (i = 0; i < width; i++)
			{
				if (m_image != NULL &&
				    (XGetPixel(m_image, i, j) == 0))
				{
					cm[m++] = 0;
				}
				else if (a_image != NULL)
				{
					cm[m++] = (unsigned char)XGetPixel(
						a_image, i, j);
					colors[k++].pixel = XGetPixel(
						image, i, j);
				}
				else
				{
					cm[m++] = 255;
					colors[k++].pixel = XGetPixel(
						image, i, j);
				}
			}
		}
		for (i = 0; i < k; i += 256)
			XQueryColors(dpy, Pcmap, &colors[i], min(k - i, 256));

		k = 0;m = 0;
		for (j = 0; j < height; j++)
		{
			for (i = 0; i < width; i++)
			{
				if (cm[m] > 0)
				{
					new_icon[l++] =
						((cm[m] & 0xff) << 24) +
						(((colors[k].red >> 8) & 0xff)
						 << 16) +
						(((colors[k].green >> 8) &
						  0xff) << 8) +
						((colors[k].blue >> 8) & 0xff);
					k++;
				}
				else
				{
					new_icon[l++] = 0;
				}
				m++;
			}
		}
		free(colors);
		free(cm);
		break;
	}
	} /* switch */


	if (is_mini_icon)
	{
		fw->ewmh_mini_icon_width = width;
		fw->ewmh_mini_icon_height = height;
	}
	else
	{
		fw->ewmh_icon_width = width;
		fw->ewmh_icon_height = height;
	}

	ewmh_ChangeProperty(
		FW_W(fw), "_NET_WM_ICON", EWMH_ATOM_LIST_PROPERTY_NOTIFY,
		(unsigned char *)new_icon, height*width + 2 + keep_length);

	if (destroy_icon_pix)
	{
		XFreePixmap(dpy, pixmap);
		if (mask != None)
		{
			XFreePixmap(dpy, mask);
		}
		if (alpha != None)
		{
			XFreePixmap(dpy, alpha);
		}
	}

	XDestroyImage(image);
	if (m_image != None)
	{
		XDestroyImage(m_image);
	}
	if (a_image != None)
	{
		XDestroyImage(a_image);
	}

	return new_icon;
}

/*
 * delete the mini icon and/or the icon from a ewmh icon
 */
void EWMH_DeleteWmIcon(FvwmWindow *fw, Bool mini_icon, Bool icon)
{
	CARD32 *list;
	CARD32 *new_list = NULL;
	int keep_start = 0, keep_length = 0;
	int s;
	int i;

	if (mini_icon && icon)
	{
		ewmh_DeleteProperty(
			FW_W(fw), "_NET_WM_ICON",
			EWMH_ATOM_LIST_PROPERTY_NOTIFY);
		fw->ewmh_mini_icon_width = 0;
		fw->ewmh_mini_icon_height = 0;
		fw->ewmh_icon_width = 0;
		fw->ewmh_icon_height = 0;
		/*SET_HAS_EWMH_WM_ICON_HINT(fw, EWMH_NO_ICON);*/
		return;
	}

	list = ewmh_AtomGetByName(
		FW_W(fw),"_NET_WM_ICON", EWMH_ATOM_LIST_PROPERTY_NOTIFY, &s);
	if (list == NULL)
	{
		return;
	}
	s = s / sizeof(CARD32);

	if (FMiniIconsSupported && list != NULL)
	{
		int k_width = (mini_icon) ? fw->ewmh_icon_width :
			fw->ewmh_mini_icon_width;
		int k_height = (mini_icon) ? fw->ewmh_icon_height :
			fw->ewmh_mini_icon_height;

		for (i = 0; i < s - 1; )
		{
			if (i + 1 + list[i]*list[i+1] < s)
			{
				if (list[i] == k_width &&
				    list[i+1] == k_height)
				{
					keep_start = i;
					keep_length =  2 + list[i]*list[i+1];
					i = s;
				}
			}
			if (i != s && list[i]*list[i+1] > 0)
			{
				i = i + 2 + list[i]*list[i+1];
			}
			else
			{
				i = s;
			}
		}
	}

	if (keep_length > 0)
	{
		new_list = fxmalloc(keep_length * sizeof(CARD32));
		memcpy(
			new_list, &list[keep_start],
			keep_length * sizeof(CARD32));
	}

	if (new_list != NULL)
	{
		ewmh_ChangeProperty(
			FW_W(fw),"_NET_WM_ICON",
			EWMH_ATOM_LIST_PROPERTY_NOTIFY,
			(unsigned char *)new_list, keep_length);
	}
	else
	{
		/*SET_HAS_EWMH_WM_ICON_HINT(fw, EWMH_NO_ICON);*/
		ewmh_DeleteProperty(
			FW_W(fw), "_NET_WM_ICON",
			EWMH_ATOM_LIST_PROPERTY_NOTIFY);
	}

	if (mini_icon)
	{
		fw->ewmh_mini_icon_width = 0;
		fw->ewmh_mini_icon_height = 0;
	}
	if (icon)
	{
		fw->ewmh_icon_width = 0;
		fw->ewmh_icon_height = 0;
	}
	if (new_list != NULL)
	{
		free(new_list);
	}
	free(list);
}

/*
 * Create an x image from a NET icon
 */

#define EWMH_ICON_MAX_DIMENSION 4096U
#define EWMH_ICON_MAX_PIXELS (16U * 1024U * 1024U)
static
void extract_wm_icon(
	CARD32 *list, int size, int wanted_w, int wanted_h,
	int *start_best, int *best_w, int *best_h)
{
	size_t pixel_start;
	uint32_t width;
	uint32_t height;

	*start_best = 0;
	*best_w = 0;
	*best_h = 0;
	if (list == NULL || size < 0)
	{
		return;
	}
	if (!FEwmhIconFindBest(
		    (const uint32_t *)list, (size_t)size / sizeof(CARD32),
		    (uint32_t)wanted_w, (uint32_t)wanted_h,
		    EWMH_ICON_MAX_DIMENSION, EWMH_ICON_MAX_PIXELS,
		    &pixel_start, &width, &height))
	{
		return;
	}
	*start_best = (int)pixel_start;
	*best_w = (int)width;
	*best_h = (int)height;

	return;
}

#define MINI_ICON_WANTED_WIDTH  16
#define MINI_ICON_WANTED_HEIGHT 16
#define MINI_ICON_MAX_WIDTH 22
#define MINI_ICON_MAX_HEIGHT 22
#define ICON_WANTED_WIDTH 56
#define ICON_WANTED_HEIGHT 56
#define ICON_MAX_WIDTH 100
#define ICON_MAX_HEIGHT 100

static monitor_scale_factor
icon_ui_factor(FvwmWindow *fw)
{
	monitor_scale_factor factor;
	struct monitor *mon;
	rectangle g;
	int x;
	int y;
	Window child;

	factor.quarters = MONITOR_SCALE_QUARTERS_MIN;
	mon = NULL;
	if (fw != NULL)
	{
		get_icon_geometry(fw, &g);
		if (g.width > 0 && g.height > 0)
		{
			mon = FindScreenOfXY(
				g.x + g.width / 2, g.y + g.height / 2);
		}
	}
	if (mon == NULL)
	{
		x = 0;
		y = 0;
		child = None;
		FQueryPointer(
			dpy, Scr.Root, &JunkRoot, &child, &x, &y,
			&JunkX, &JunkY, &JunkMask);
		mon = FindScreenOfXY(x, y);
	}
	if (mon != NULL)
	{
		factor = mon->effective_scale.factor;
	}

	return factor;
}

static int
scale_icon_source(int source, monitor_scale_factor factor)
{
	int scaled;
	monitor_scale_conversion_status status;

	scaled = source;
	status = wm_ui_scale_px(source, factor, &scaled);
	if (status != MONITOR_SCALE_CONVERSION_OK)
	{
		fvwm_debug(__func__,
			"failed to scale icon size %d (%d); keeping source",
			source, (int)status);
		return source;
	}

	return scaled;
}

int EWMH_SetIconFromWMIcon(
	FvwmWindow *fw, CARD32 *list, int size, Bool is_mini_icon)
{
	int start, width, height;
	int wanted_w, wanted_h;
	int max_w, max_h;
	Pixmap pixmap = None;
	Pixmap mask = None;
	Pixmap alpha = None;
	Bool free_list = False;
	int nalloc_pixels;
	Pixel *alloc_pixels;
	int no_limit;
	FvwmPictureAttributes fpa;

	if (list == NULL)
	{
		/* we are called from icons.c or update.c */
		list = ewmh_AtomGetByName(
			FW_W(fw),"_NET_WM_ICON",
			EWMH_ATOM_LIST_PROPERTY_NOTIFY, &size);
		free_list = True;
		if (list == NULL)
		{
			return 0;
		}
	}

	{
		monitor_scale_factor factor;

		factor = icon_ui_factor(fw);
		if (is_mini_icon)
		{
			wanted_w = scale_icon_source(
				MINI_ICON_WANTED_WIDTH, factor);
			wanted_h = scale_icon_source(
				MINI_ICON_WANTED_HEIGHT, factor);
			max_w = scale_icon_source(
				MINI_ICON_MAX_WIDTH, factor);
			max_h = scale_icon_source(
				MINI_ICON_MAX_HEIGHT, factor);
			fpa.mask = 0;
		}
		else
		{
			wanted_w = scale_icon_source(
				ICON_WANTED_WIDTH, factor);
			wanted_h = scale_icon_source(
				ICON_WANTED_HEIGHT, factor);
			max_w = scale_icon_source(ICON_MAX_WIDTH, factor);
			max_h = scale_icon_source(ICON_MAX_HEIGHT, factor);
			if (fw->cs >= 0 && Colorset[fw->cs].do_dither_icon)
			{
				fpa.mask = FPAM_DITHER;
			}
			else
			{
				fpa.mask = 0;
			}
		}
	}

	extract_wm_icon(
		list, size, wanted_w, wanted_h, &start, &width, &height);
	if (width == 0 || height == 0)
	{
		if (free_list)
		{
			free(list);
		}
		return 0;
	}

	if (!PImageCreatePixmapFromArgbData(
		dpy, Scr.NoFocusWin, list, (size_t)size / sizeof(CARD32),
		start, width, height,
		&pixmap, &mask, &alpha, &nalloc_pixels,
		&alloc_pixels, &no_limit, fpa))
	{
		fvwm_debug(__func__,
			   "fail to create a pixmap\n");
		if (free_list)
		{
			free(list);
		}
		return 0;
	}

	if (width > max_w || height > max_h)
	{
		Pixmap np = None,nm =None, na = None;

		if (pixmap)
		{
			np = CreateStretchPixmap(
				dpy, pixmap, width, height, Pdepth, wanted_w,
				wanted_h, Scr.TitleGC);
			XFreePixmap(dpy, pixmap);
			pixmap = np;
		}
		if (mask)
		{
			nm = CreateStretchPixmap(
				dpy, mask, width, height, 1, wanted_w,
				wanted_h, Scr.MonoGC);
			XFreePixmap(dpy, mask);
			mask = nm;
		}
		if (alpha)
		{
			na = CreateStretchPixmap(
				dpy, alpha, width, height,
				FRenderGetAlphaDepth(), wanted_w, wanted_h,
				Scr.AlphaGC);
			XFreePixmap(dpy, alpha);
			alpha = na;
		}
		width = wanted_w;
		height = wanted_h;
	}
	if (FMiniIconsSupported && is_mini_icon &&
	    !DO_EWMH_MINI_ICON_OVERRIDE(fw))
	{
		char *name = NULL;

		CopyString(&name,"ewmh_mini_icon");
		if (fw->mini_icon)
		{
			PDestroyFvwmPicture(dpy,fw->mini_icon);
			fw->mini_icon = 0;
		}
		fw->mini_icon = PCacheFvwmPictureFromPixmap(
			dpy, Scr.NoFocusWin, name, pixmap,mask,alpha, width,
			height, nalloc_pixels, alloc_pixels, no_limit);
		if (fw->mini_icon != NULL)
		{
			fw->mini_pixmap_file = name;
			BroadcastFvwmPicture(
				M_MINI_ICON, FW_W(fw), FW_W_FRAME(fw),
				(unsigned long)fw, fw->mini_icon,
				fw->mini_pixmap_file);
			border_redraw_decorations(fw);
		}
	}
	if (!is_mini_icon)
	{
		fw->iconPixmap = pixmap;
		fw->icon_maskPixmap = mask;
		fw->icon_alphaPixmap = alpha;
		fw->icon_nalloc_pixels = nalloc_pixels;
		fw->icon_alloc_pixels = alloc_pixels;
		fw->icon_no_limit = no_limit;
		fw->icon_g.picture_w_g.width = width;
		fw->icon_g.picture_w_g.height = height;
		fw->iconDepth = Pdepth;
		SET_PIXMAP_OURS(fw, 1);
		if (FShapesSupported && mask)
		{
			SET_ICON_SHAPED(fw, 1);
		}
	}
	if (free_list)
	{
		free(list);
	}
	return 1;
}
