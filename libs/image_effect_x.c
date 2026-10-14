/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "image_effect_x.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define IMAGE_EFFECT_TILE_PIXELS 256U

static bool effect_x_error;

static int effect_error(Display *dpy, XErrorEvent *error)
{
	(void)dpy;
	(void)error;
	effect_x_error = true;
	return 0;
}

static bool effect_geometry(Display *dpy, Drawable drawable,
	const image_effect_request *request, unsigned expected_depth)
{
	Window root;
	int x, y;
	unsigned width, height, border, depth;

	return XGetGeometry(dpy, drawable, &root, &x, &y, &width, &height,
	    &border, &depth) && !effect_x_error && width == request->width &&
	    height == request->height && depth == expected_depth;
}

bool image_effect_render(const char *display,
	const image_effect_request *request, image_effect_write_fn write_result)
{
	unsigned char header[IMAGE_EFFECT_HEADER_SIZE];
	unsigned char checked[IMAGE_EFFECT_REQUEST_SIZE];

	if (display == NULL || write_result == NULL ||
	    !image_effect_request_encode(checked, request))
	{
		return false;
	}
	Display *dpy = XOpenDisplay(display);

	if (dpy == NULL)
	{
		return false;
	}
	bool success = false;
	int (*previous_error)(Display *, XErrorEvent *) =
	    XSetErrorHandler(effect_error);
	effect_x_error = false;
	XImage *source = NULL, *mask = NULL, *alpha = NULL;
	XVisualInfo template = {.visualid = request->visual};
	int count;
	XVisualInfo *visual = XGetVisualInfo(dpy, VisualIDMask, &template, &count);

	if (visual == NULL || count != 1 || visual->depth != request->depth)
	{
		XFree(visual);
		goto done;
	}
	XFree(visual);
	if (!effect_geometry(dpy, request->pixmap, request, request->depth) ||
	    (request->mask &&
	    !effect_geometry(dpy, request->mask, request, 1)) ||
	    (request->alpha && !effect_geometry(dpy, request->alpha, request,
	    request->alpha_depth)))
	{
		goto done;
	}
	XColor background = {.pixel = request->background};
	XColor tint = {.pixel = request->tint};

	if (!XQueryColor(dpy, request->colormap, &background) ||
	    (request->tint_percent &&
	    !XQueryColor(dpy, request->colormap, &tint)) || effect_x_error)
	{
		goto done;
	}
	uint16_t background_rgb[] = {background.red, background.green,
	    background.blue};
	uint16_t tint_rgb[] = {tint.red, tint.green, tint.blue};

	image_effect_header(header, request->width, request->height);
	if (!write_result(header, sizeof(header)))
	{
		goto done;
	}
	for (unsigned y = 0; y < request->height; y++)
	{
		for (unsigned x = 0; x < request->width; )
		{
			unsigned width = request->width - x;

			if (width > IMAGE_EFFECT_TILE_PIXELS)
			{
				width = IMAGE_EFFECT_TILE_PIXELS;
			}
			source = XGetImage(dpy, request->pixmap, x, y, width, 1,
			    AllPlanes, ZPixmap);
			if (request->mask)
			{
				mask = XGetImage(dpy, request->mask, x, y, width, 1,
				    AllPlanes, ZPixmap);
			}
			if (request->alpha)
			{
				alpha = XGetImage(dpy, request->alpha, x, y,
				    width, 1, AllPlanes, ZPixmap);
			}
			if (source == NULL || (request->mask && mask == NULL) ||
			    (request->alpha && alpha == NULL) || effect_x_error)
			{
				goto done;
			}
			XColor colors[IMAGE_EFFECT_TILE_PIXELS];
			unsigned char output[IMAGE_EFFECT_TILE_PIXELS *
			    IMAGE_EFFECT_RECORD_SIZE];

			for (unsigned i = 0; i < width; i++)
			{
				colors[i].pixel = XGetPixel(source, i, 0);
			}
			if (!XQueryColors(dpy, request->colormap, colors, width) ||
			    effect_x_error)
			{
				goto done;
			}
			for (unsigned i = 0; i < width; i++)
			{
				uint16_t rgb[] = {colors[i].red, colors[i].green,
				    colors[i].blue};
				unsigned opacity = alpha ?
				    XGetPixel(alpha, i, 0) : 255;
				bool masked = mask && XGetPixel(mask, i, 0) == 0;
				image_effect_pixel pixel;

				if (!image_effect_blend(rgb, tint_rgb,
				    background_rgb, opacity, masked,
				    request->tint_percent, request->alpha_percent,
				    &pixel))
				{
					goto done;
				}
				image_effect_record(output +
				    i * IMAGE_EFFECT_RECORD_SIZE, &pixel);
			}
			XDestroyImage(source);
			source = NULL;
			if (mask != NULL)
			{
				XDestroyImage(mask);
				mask = NULL;
			}
			if (alpha != NULL)
			{
				XDestroyImage(alpha);
				alpha = NULL;
			}
			if (!write_result(output, width * IMAGE_EFFECT_RECORD_SIZE))
			{
				goto done;
			}
			x += width;
		}
	}
	success = true;
done:
	if (source != NULL)
	{
		XDestroyImage(source);
	}
	if (mask != NULL)
	{
		XDestroyImage(mask);
	}
	if (alpha != NULL)
	{
		XDestroyImage(alpha);
	}
	XCloseDisplay(dpy);
	XSetErrorHandler(previous_error);
	return success && !effect_x_error;
}
