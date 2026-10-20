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
  Changed 02/12/97 by Dan Espen:
  - added routines to determine color closeness, for color use reduction.
  Some of the logic comes from pixy2, so the copyright is below.
*/

/*
 *
 * Routines to handle initialization, loading, and removing of xpm's or mono-
 * icon images.
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdint.h>

#include <X11/Xlib.h>

#include "fvwmlib.h"
#include "Graphics.h"
#include "PictureBase.h"
#include "PictureUtils.h"
#include "Fsvg.h"
#include "Strings.h"
#include "log.h"

Bool Pdefault;
Visual *Pvisual;
static Visual *FvwmVisual;
Colormap Pcmap;
static Colormap FvwmCmap;
unsigned int Pdepth;
static unsigned int FvwmDepth;
Display *Pdpy;            /* Save area for display pointer */
Bool PUseDynamicColors;

Pixel PWhitePixel;
Pixel PBlackPixel;
Pixel FvwmWhitePixel;
Pixel FvwmBlackPixel;

static Bool picture_parse_xid(const char *text, unsigned long *value)
{
	uint32_t parsed = 0;
	unsigned int digits = 0;

	*value = None;
	if (text == NULL)
		return False;
	if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
		text += 2;
	while (*text != '\0' && digits < 8)
	{
		unsigned int digit;

		if (*text >= '0' && *text <= '9')
			digit = *text - '0';
		else if (*text >= 'a' && *text <= 'f')
			digit = *text - 'a' + 10;
		else if (*text >= 'A' && *text <= 'F')
			digit = *text - 'A' + 10;
		else
			return False;
		parsed = (parsed << 4) | digit;
		digits++;
		text++;
	}
	if (digits == 0 || *text != '\0' || parsed == None)
		return False;
	*value = parsed;
	return True;
}

static void picture_set_default_visual(Display *dpy)
{
	int screen = DefaultScreen(dpy);

	Pvisual = DefaultVisual(dpy, screen);
	Pdepth = DefaultDepth(dpy, screen);
	Pcmap = DefaultColormap(dpy, screen);
	Pdefault = True;
}

void PictureInitCMap(Display *dpy)
{
	const char *visual_text = getenv("FVWM_VISUALID");
	const char *colormap_text = getenv("FVWM_COLORMAP");
	unsigned long visual_id, colormap;

	Pdpy = dpy;
	picture_set_default_visual(dpy);
	/* The module producer supplies two nonzero hexadecimal X protocol IDs.
	 * Admit the complete pair before querying or publishing either value. */
	if (visual_text != NULL || colormap_text != NULL)
	{
		if (picture_parse_xid(visual_text, &visual_id) &&
		    picture_parse_xid(colormap_text, &colormap))
		{
			int count = 0;
			XVisualInfo query = {0}, *reply;

			query.visualid = visual_id;
			query.screen = DefaultScreen(dpy);
			reply = XGetVisualInfo(dpy, VisualIDMask | VisualScreenMask,
				&query, &count);
			if (reply != NULL && count > 0 && reply->visual != NULL &&
			    reply->depth > 0)
			{
				/* Visual belongs to the Display; only the reply is owned. */
				Pvisual = reply->visual;
				Pdepth = reply->depth;
				Pcmap = colormap;
				Pdefault = False;
			}
			if (reply != NULL)
				XFree(reply);
		}
		if (Pdefault)
			fvwm_debug(__func__, "Invalid inherited visual/colormap; "
				"using screen defaults");
	}

	if (!PictureSetupWhiteAndBlack())
	{
		fvwm_debug(__func__, "Cannot allocate inherited visual colors; "
			"using screen defaults");
		picture_set_default_visual(dpy);
		PictureSetupWhiteAndBlack();
	}
	PictureSaveFvwmVisual();

	/* initialise color limit */
	PUseDynamicColors = 0;
	PictureInitColors(PICTURE_CALLED_BY_MODULE, True, NULL, False, True);
	return;
}

void PictureInitCMapRoot(
	Display *dpy, Bool init_color_limit, PictureColorLimitOption *opt,
	Bool use_my_color_limit, Bool init_dither)
{
	Pdpy = dpy;
	picture_set_default_visual(dpy);

	PictureSetupWhiteAndBlack();
	PictureSaveFvwmVisual();

	/* initialise color limit */
	PictureInitColors(
		PICTURE_CALLED_BY_MODULE, init_color_limit, opt,
		use_my_color_limit, init_dither);
	return;
}

Bool PictureSetupWhiteAndBlack(void)
{
	XColor white = {0}, black = {0};

	if (!Pdefault)
	{
		white.flags = black.flags = DoRed | DoGreen | DoBlue;
		white.red = white.green = white.blue = 65535;
		if (!XAllocColor(Pdpy, Pcmap, &white))
			return False;
		if (!XAllocColor(Pdpy, Pcmap, &black))
		{
			XFreeColors(Pdpy, Pcmap, &white.pixel, 1, 0);
			return False;
		}
		/* Commit both pixels only after both allocations succeed. */
		PWhitePixel = white.pixel;
		PBlackPixel = black.pixel;
	}
	else
	{
		PWhitePixel = WhitePixel(Pdpy, DefaultScreen(Pdpy));
		PBlackPixel = BlackPixel(Pdpy, DefaultScreen(Pdpy));
	}

	return True;
}

void PictureUseDefaultVisual(void)
{
	int screen = DefaultScreen(Pdpy);

	Pvisual = DefaultVisual(Pdpy, screen);
	Pdepth = DefaultDepth(Pdpy, screen);
	Pcmap = DefaultColormap(Pdpy, screen);
	PWhitePixel = WhitePixel(Pdpy, DefaultScreen(Pdpy));
	PBlackPixel = BlackPixel(Pdpy, DefaultScreen(Pdpy));
	return;
}

void PictureUseFvwmVisual(void)
{
	Pvisual = FvwmVisual;
	Pdepth = FvwmDepth;
	Pcmap = FvwmCmap;
	PWhitePixel = FvwmWhitePixel;
	PBlackPixel = FvwmBlackPixel;
	return;
}

void PictureSaveFvwmVisual(void)
{
	FvwmVisual = Pvisual;
	FvwmDepth = Pdepth;
	FvwmCmap = Pcmap;
	FvwmWhitePixel = PWhitePixel;
	FvwmBlackPixel = PBlackPixel;
	return;
}

Pixel PictureWhitePixel(void)
{
	return PWhitePixel;
}

Pixel PictureBlackPixel(void)
{
	return PBlackPixel;
}

GC PictureDefaultGC(Display *dpy, Window win)
{
	static GC gc = None;

	if (Pdepth == DefaultDepth(dpy, DefaultScreen(dpy)))
	{
		return DefaultGC(dpy, DefaultScreen(dpy));
	}
	if (gc == None)
	{
		gc = fvwmlib_XCreateGC(dpy, win, 0, NULL);
	}

	return gc;
}

static char* imagePath = FVWM_IMAGEPATH;

void PictureSetImagePath( const char* newpath )
{
	static int need_to_free = 0;
	setPath( &imagePath, newpath, need_to_free );
	need_to_free = 1;

	return;
}

char* PictureGetImagePath(void)
{
	return imagePath;
}

/*
 *
 * Find the specified image file somewhere along the given path.
 *
 * There is a possible race condition here:  We check the file and later
 * do something with it.  By then, the file might not be accessible.
 * Oh well.
 *
 */
char* PictureFindImageFile(const char* icon, const char* pathlist, int type)
{
	int length;
	char *tmpbuf;
	char *full_filename;
	const char *render_opts;

	if (pathlist == NULL)
	{
		pathlist = imagePath;
	}
	if (icon == NULL)
	{
		return NULL;
	}

	full_filename = searchPath(pathlist, icon, ".gz", type);

	/* With USE_SVG, rendering options may be appended to the
	   original filename, hence seachPath() won't find the file.
	   So we hide any such appended options and try once more. */
        if (USE_SVG && !full_filename &&
	    (render_opts = strrchr(icon, ':')))
	{
		length = render_opts - icon;
		/* TA:  FIXME!  asprintF() */
		tmpbuf = fxmalloc(length + 1);
		strncpy(tmpbuf, icon, length);
		tmpbuf[length] = 0;

		full_filename = searchPath(pathlist, tmpbuf, ".gz", type);
		free(tmpbuf);
		if (full_filename)
		{
			/* Prepending (the previously appended) options
			   will leave any file suffix exposed. Callers
			   who want to access the file on disk will have
			   to remove these prepended options themselves.
			   The format is ":svg_opts:/path/to/file.svg". */
			xasprintf(&tmpbuf, "%s:%s", render_opts, full_filename);
			free(full_filename);
			full_filename = fxstrdup(tmpbuf);
			free(tmpbuf);
		}
	}

	return full_filename;
}
