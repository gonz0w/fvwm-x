/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include "libs/flocale_font_types.h"
#include "libs/FftInterface.h"
#include "libs/PictureBase.h"
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#define CLIP_TEST_WIDTH 256
#define CLIP_TEST_HEIGHT 128

static Bool clip_test_fail_region;
static unsigned int clip_test_region_calls, clip_test_x_errors;

Region __real_XCreateRegion(void);
Region __wrap_XCreateRegion(void);

Region __wrap_XCreateRegion(void)
{
	clip_test_region_calls++;
	if (clip_test_fail_region)
	{
		clip_test_fail_region = False;
		return NULL;
	}
	return __real_XCreateRegion();
}

static int clip_test_error(Display *display, XErrorEvent *error)
{
	(void)display;
	clip_test_x_errors++;
	fprintf(stderr, "text clip X error %u request %u\n",
	    error->error_code, error->request_code);
	return 0;
}

static void clear_surface(Display *display, Window window, GC gc)
{
	XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
	XFillRectangle(display, window, gc, 0, 0,
	    CLIP_TEST_WIDTH, CLIP_TEST_HEIGHT);
	XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
	XSync(display, False);
}

static unsigned long pixels(Display *display, Window window,
	const XRectangle *clip)
{
	XImage *image = XGetImage(display, window, 0, 0, CLIP_TEST_WIDTH,
	    CLIP_TEST_HEIGHT, AllPlanes, ZPixmap);
	unsigned long count = 0;
	Pixel background = BlackPixel(display, DefaultScreen(display));

	assert(image);
	for (int y = 0; y < CLIP_TEST_HEIGHT; y++)
		for (int x = 0; x < CLIP_TEST_WIDTH; x++)
		{
			if (XGetPixel(image, x, y) == background)
				continue;
			assert(clip && x >= clip->x && y >= clip->y &&
			    x < clip->x + clip->width &&
			    y < clip->y + clip->height);
			count++;
		}
	XDestroyImage(image);
	return count;
}

static unsigned long clipped_draw(Display *display, FlocaleFont *font,
	FlocaleWinString *string, XRectangle clip, Region previous)
{
	clear_surface(display, string->win, string->gc);
	string->clip_region = previous;
	string->flags.has_clip_region = True;
	assert(flocale_draw_string_clipped(display, font, string,
	    FWS_HAVE_LENGTH, clip));
	assert(string->clip_region == previous &&
	    string->flags.has_clip_region);
	unsigned long count = pixels(display, string->win, &clip);
	assert(count > 0);
	return count;
}

int main(int argc, char **argv)
{
	Bool xft = argc == 2 && !strcmp(argv[1], "--xft");
	Display *display;
	const char *backend;
	FlocaleFont *font;
	XGCValues values;
	unsigned long mask = GCForeground | GCBackground;
	char text[] = "MMMMMMMMMMMMMMMMMMMMMMMM";
	unsigned long ink = 0;
	int screen;

	if (argc != 1 && !xft)
		return 2;
	FlocaleInit(LC_CTYPE, xft ? "" : "C", NULL, "text-clip-test");
	display = XOpenDisplay(NULL);
	if (!display)
		return 77;
	XSetErrorHandler(clip_test_error);
	PictureInitCMap(display);
	screen = DefaultScreen(display);
	font = FlocaleLoadFont(display, xft ? "xft:Sans:size=18" : "fixed",
	    "text-clip-test");
	assert(font && font->height > 2 && font->height < 80);
	if (xft)
	{
		assert(font->fftf.fftfont[0] != NULL);
		backend = "xft";
	}
	else
	{
		assert(font->fftf.fftfont[0] == NULL);
		assert((font->font != NULL) != (font->fontset != NULL));
		backend = font->fontset ? "fontset" : "core-font";
	}
	Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
	    0, 0, CLIP_TEST_WIDTH, CLIP_TEST_HEIGHT, 0,
	    BlackPixel(display, screen), BlackPixel(display, screen));
	XMapWindow(display, window);
	values.foreground = WhitePixel(display, screen);
	values.background = BlackPixel(display, screen);
	if (font->font)
	{
		values.font = font->font->fid;
		mask |= GCFont;
	}
	GC gc = XCreateGC(display, window, mask, &values);
	FlocaleWinString string = {.str = text, .gc = gc, .win = window,
		.x = 8, .y = 8 + font->ascent, .len = sizeof(text) - 1};
	Region previous = XCreateRegion();
	XRectangle old_clip = {100, 100, 1, 1};
	assert(gc && previous);
	XUnionRectWithRegion(&old_clip, previous, previous);
	XRectangle clip = {17, 8, 19, (unsigned short)(font->height / 2 + 1)};
	ink += clipped_draw(display, font, &string, clip, previous);
	clip.x = -4;
	clip.width = 23;
	ink += clipped_draw(display, font, &string, clip, previous);

	/* A later ordinary text draw and GC operation must reach outside the
	 * helper's rectangle. This checks both backend and GC clip reuse. */
	string.flags.has_clip_region = False;
	string.clip_region = NULL;
	string.x = 180;
	string.str = text;
	string.len = 2;
	clear_surface(display, window, gc);
	FlocaleDrawString(display, font, &string, FWS_HAVE_LENGTH);
	XRectangle ordinary = {180, 8, 76, (unsigned short)font->height};
	assert(pixels(display, window, &ordinary) > 0);
	XFillRectangle(display, window, gc, 250, 120, 2, 2);
	XImage *corner = XGetImage(display, window, 250, 120, 1, 1,
	    AllPlanes, ZPixmap);
	assert(corner && XGetPixel(corner, 0, 0) == WhitePixel(display, screen));
	XDestroyImage(corner);

	clear_surface(display, window, gc);
	string.clip_region = previous;
	string.flags.has_clip_region = True;
	unsigned int calls = clip_test_region_calls;
	unsigned long serial = NextRequest(display);
	clip.width = 0;
	assert(flocale_draw_string_clipped(display, font, &string, 0, clip));
	clip.width = 10;
	clip.height = 0;
	assert(flocale_draw_string_clipped(display, font, &string, 0, clip));
	assert(clip_test_region_calls == calls && NextRequest(display) == serial);
	assert(string.clip_region == previous && string.flags.has_clip_region);

	clip.height = 10;
	clip_test_fail_region = True;
	assert(!flocale_draw_string_clipped(display, font, &string, 0, clip));
	assert(!clip_test_fail_region && clip_test_region_calls == calls + 1);
	assert(NextRequest(display) == serial);
	assert(string.clip_region == previous && string.flags.has_clip_region);
	assert(pixels(display, window, NULL) == 0);
	assert(!flocale_draw_string_clipped(display, NULL, &string, 0, clip));
	assert(!flocale_draw_string_clipped(display, font, NULL, 0, clip));
	assert(!flocale_draw_string_clipped(NULL, font, &string, 0, clip));

	XDestroyRegion(previous);
	XFreeGC(display, gc);
	XDestroyWindow(display, window);
	FlocaleUnloadFont(display, font);
	XSync(display, False);
	assert(clip_test_x_errors == 0);
	XCloseDisplay(display);
	FftFini();
	printf("{\"backend\":\"%s\",\"clipped_rectangles\":2,"
	    "\"ink_pixels\":%lu,\"descriptor_restored\":true,"
	    "\"unclipped_reuse\":true,\"empty_noop\":true,"
	    "\"allocation_failure_unchanged\":true,\"x_errors\":0}\n",
	    backend, ink);
	return 0;
}
