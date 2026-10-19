/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "picture_argb.h"
#include "picture_average.h"
#include "Picture.h"
#include "PictureUtils.h"
#include "FRenderInit.h"
#include "ImageSafety.h"
#include "image_svg_protocol.h"
#include "safemalloc.h"

#include <errno.h>
#include <stdlib.h>
#include "libs/picture_types.h"
struct picture_argb_builder
{
	Display *dpy;
	Window drawable;
	const uint32_t *pixels;
	const image_effect_pixel *effects;
	Pixel background;
	FvwmPicture *picture;
	PictureImageColorAllocator *allocator;
	XImage *color;
	XImage *mask;
	XImage *alpha;
	GC color_gc;
	GC mask_gc;
	GC alpha_gc;
	unsigned tile_width;
	unsigned tile_height;
	unsigned step_pixels;
	unsigned x;
	unsigned y;
	int alpha_limit;
	unsigned long pixmap_serials[3];
	picture_average average;
	Bool argb32;
	Bool have_mask;
	Bool have_alpha;
	Bool cancelled;
	Bool done;
};

static XImage *picture_argb_image(Display *dpy, unsigned depth,
	unsigned width, unsigned height)
{
	XImage *image = XCreateImage(dpy, Pvisual, depth, ZPixmap, 0, NULL,
	    width, height, Pdepth > 16 ? 32 : Pdepth > 8 ? 16 : 8, 0);

	if (image == NULL)
	{
		return NULL;
	}
	if (image->bytes_per_line <= 0 ||
	    (size_t)image->bytes_per_line > SIZE_MAX / height)
	{
		XDestroyImage(image);
		errno = EOVERFLOW;
		return NULL;
	}
	image->data = fxcalloc(height, image->bytes_per_line);
	return image;
}

/* Lazy auxiliary pixmaps begin with the opaque value for preceding tiles.
 * Each following tile then supplies its actual mask/alpha values. */
static void picture_argb_aux(picture_argb_builder *builder, Pixmap *pixmap,
	GC *gc, unsigned depth, unsigned long opaque)
{
	if (*pixmap != None)
	{
		return;
	}
	FvwmPicture *picture = builder->picture;
	XGCValues values;

	unsigned index = pixmap == &picture->mask ? 1 : 2;

	builder->pixmap_serials[index] = NextRequest(builder->dpy);
	*pixmap = XCreatePixmap(builder->dpy, builder->drawable,
	    picture->width, picture->height, depth);
	values.foreground = opaque;
	*gc = XCreateGC(builder->dpy, *pixmap, GCForeground, &values);
	XFillRectangle(builder->dpy, *pixmap, *gc, 0, 0,
	    picture->width, picture->height);
}

static picture_argb_builder *picture_argb_begin(Display *dpy, Window drawable,
	const uint32_t *pixels, const image_effect_pixel *effects, size_t length,
	unsigned width, unsigned height, FvwmPictureAttributes attributes,
	Bool argb32, Pixel background)
{
	size_t count;

	if ((pixels == NULL && effects == NULL) || !FImageSizeIsSafe(width, height,
	    IMAGE_SVG_MAX_DIMENSION, IMAGE_SVG_MAX_PIXELS, &count) ||
	    count > length)
	{
		errno = EINVAL;
		return NULL;
	}
	picture_argb_builder *builder = fxcalloc(1, sizeof(*builder));
	FvwmPicture *picture = fxcalloc(1, sizeof(*picture));
	unsigned alpha_depth = FRenderGetAlphaDepth();

	builder->dpy = dpy;
	builder->drawable = drawable;
	builder->pixels = pixels;
	builder->effects = effects;
	builder->background = background;
	builder->argb32 = argb32;
	builder->picture = picture;
	/* Dynamic color allocation can issue a round trip for each pixel.
	 * Keep that service budget smaller than direct-color conversion. */
	builder->step_pixels = !argb32 && Pdepth <= 8 ? 256 :
	    PICTURE_ARGB_STEP_PIXELS;
	builder->tile_width = width < builder->step_pixels ?
	    width : builder->step_pixels;
	builder->tile_height = builder->step_pixels / builder->tile_width;
	if (builder->tile_height > height)
	{
		builder->tile_height = height;
	}
	picture->count = 1;
	picture->width = width;
	picture->height = height;
	picture->depth = argb32 ? 32 :
	    attributes.mask & FPAM_MONOCHROME ? 1 : Pdepth;
	picture->fpa_mask = attributes.mask;
	picture->color_cmap = Pcmap;
	builder->alpha_limit = PICTURE_ALPHA_LIMIT;
	builder->color = picture_argb_image(dpy, picture->depth,
	    builder->tile_width, builder->tile_height);
	if (!argb32 && effects == NULL)
	{
		builder->mask = picture_argb_image(dpy, 1,
		    builder->tile_width, builder->tile_height);
	}
	if (!argb32 && effects == NULL &&
	    !(attributes.mask & FPAM_NO_ALPHA) && alpha_depth != 0)
	{
		builder->alpha_limit = 0;
		builder->alpha = picture_argb_image(dpy, alpha_depth,
		    builder->tile_width, builder->tile_height);
		if (builder->alpha == NULL)
		{
			goto failed;
		}
	}
	if (builder->color == NULL ||
	    (!argb32 && effects == NULL && builder->mask == NULL))
	{
		goto failed;
	}
	if (!argb32 && !(attributes.mask & FPAM_MONOCHROME))
	{
		builder->allocator = PictureOpenImageColorAllocator(dpy, Pcmap,
		    width, height, !!(attributes.mask & FPAM_NO_COLOR_LIMIT),
		    !!(attributes.mask & FPAM_NO_ALLOC_PIXELS),
		    !!(attributes.mask & FPAM_DITHER), effects == NULL);
	}
	builder->pixmap_serials[0] = NextRequest(dpy);
	picture->picture = XCreatePixmap(dpy, drawable, width, height,
	    picture->depth);
	builder->color_gc = XCreateGC(dpy, picture->picture, 0, NULL);
	return builder;

failed:
	builder->cancelled = builder->done = True;
	picture_argb_destroy(builder);
	return NULL;
}

picture_argb_builder *picture_argb_start(Display *dpy, Window drawable,
	const uint32_t *pixels, size_t length, unsigned width, unsigned height,
	FvwmPictureAttributes attributes)
{
	return picture_argb_begin(dpy, drawable, pixels, NULL, length, width,
	    height, attributes, False, 0);
}

picture_argb_builder *picture_argb_start_argb32(Display *dpy,
	Window drawable, const uint32_t *pixels, size_t length,
	unsigned width, unsigned height)
{
	return picture_argb_begin(dpy, drawable, pixels, NULL, length, width,
	    height, (FvwmPictureAttributes){0}, True, 0);
}

picture_argb_builder *picture_argb_start_effects(Display *dpy, Window drawable,
	const image_effect_pixel *pixels, size_t length, unsigned width,
	unsigned height, Pixel background)
{
	return picture_argb_begin(dpy, drawable, NULL, pixels, length, width,
	    height, (FvwmPictureAttributes){.mask = FPAM_NO_ALPHA}, False,
	    background);
}

static void picture_argb_convert(picture_argb_builder *builder)
{
	FvwmPicture *picture = builder->picture;
	unsigned width = picture->width - builder->x;
	unsigned height = picture->height - builder->y;

	if (width > builder->tile_width)
	{
		width = builder->tile_width;
	}
	if (height > builder->tile_height)
	{
		height = builder->tile_height;
	}
	for (unsigned y = 0; y < height; y++)
	{
		for (unsigned x = 0; x < width; x++)
		{
			unsigned source_x = builder->x + x;
			unsigned source_y = builder->y + y;
			if (builder->effects != NULL)
			{
				const image_effect_pixel *effect = &builder->effects[
				    (size_t)source_y * picture->width + source_x];
				XColor color = {.red = effect->red,
				    .green = effect->green, .blue = effect->blue,
				    .flags = DoRed | DoGreen | DoBlue};

				if (effect->kind == IMAGE_EFFECT_BACKGROUND)
				{
					color.pixel = builder->background;
				}
				else if (effect->kind != IMAGE_EFFECT_RGB ||
				    !PictureAllocColorImage(builder->dpy,
				    builder->allocator, &color, source_x, source_y))
				{
					builder->cancelled = True;
					return;
				}
				XPutPixel(builder->color, x, y, color.pixel);
				continue;
			}
			uint32_t pixel = builder->pixels[
			    (size_t)source_y * picture->width + source_x];
			unsigned alpha = pixel >> 24;

			if (!builder->argb32 &&
			    !(picture->fpa_mask & FPAM_MONOCHROME) &&
			    !picture_average_add(&builder->average, pixel))
			{
				builder->cancelled = True;
				return;
			}

			if (builder->argb32)
			{
				uint32_t red =
				    ((pixel >> 16) & 255) * alpha / 255;
				uint32_t green =
				    ((pixel >> 8) & 255) * alpha / 255;
				uint32_t blue = (pixel & 255) * alpha / 255;

				XPutPixel(builder->color, x, y,
				    (alpha << 24) | (red << 16) |
				    (green << 8) | blue);
				continue;
			}
			XColor color = {.flags = DoRed | DoGreen | DoBlue};

			if (alpha > builder->alpha_limit)
			{
				color.red = (pixel >> 16) & 0xff;
				color.green = (pixel >> 8) & 0xff;
				color.blue = pixel & 0xff;
				if (builder->allocator != NULL)
				{
					PictureAllocColorImage(builder->dpy,
					    builder->allocator, &color,
					    source_x, source_y);
				}
				else
				{
					color.pixel = (0x99 * color.red +
					    0x12d * color.green +
					    0x3a * color.blue) >> 16 ? 1 : 0;
				}
				XPutPixel(builder->color, x, y, color.pixel);
				XPutPixel(builder->mask, x, y, 1);
			}
			else
			{
				/* Clear unspecified transparent RGB to avoid
				 * carrying pixels from a preceding tile. */
				XPutPixel(builder->color, x, y, 0);
				XPutPixel(builder->mask, x, y, 0);
				builder->have_mask = True;
			}
			if (builder->alpha != NULL)
			{
				XPutPixel(builder->alpha, x, y, alpha);
				if (alpha > 0 && alpha < 255)
				{
					builder->have_alpha = True;
				}
			}
		}
	}
	XPutImage(builder->dpy, picture->picture, builder->color_gc,
	    builder->color, 0, 0, builder->x, builder->y, width, height);
	if (builder->alpha != NULL &&
	    (builder->have_alpha || builder->have_mask))
	{
		picture_argb_aux(builder, &picture->alpha, &builder->alpha_gc,
		    builder->alpha->depth, 255);
		XPutImage(builder->dpy, picture->alpha, builder->alpha_gc,
		    builder->alpha, 0, 0, builder->x, builder->y,
		    width, height);
	}
	if (builder->have_mask)
	{
		picture_argb_aux(builder, &picture->mask, &builder->mask_gc,
		    1, 1);
		XPutImage(builder->dpy, picture->mask, builder->mask_gc,
		    builder->mask, 0, 0, builder->x, builder->y, width, height);
	}
	builder->x += width;
	if (builder->x == picture->width)
	{
		builder->x = 0;
		builder->y += height;
	}
}

Bool picture_argb_step(picture_argb_builder *builder)
{
	if (builder->done)
	{
		return True;
	}
	if (!builder->cancelled && builder->y < builder->picture->height)
	{
		picture_argb_convert(builder);
	}
	Bool trimmed = picture_trim_image_color_allocator(builder->dpy,
	    builder->allocator, builder->step_pixels);

	if ((builder->cancelled || builder->y == builder->picture->height) &&
	    trimmed)
	{
		if (builder->allocator != NULL)
		{
			PictureCloseImageColorAllocator(builder->dpy,
			    builder->allocator,
			    &builder->picture->nalloc_pixels,
			    &builder->picture->alloc_pixels,
			    &builder->picture->no_limit);
			builder->allocator = NULL;
		}
		if (builder->have_alpha && builder->picture->mask != None)
		{
			XFreePixmap(builder->dpy, builder->picture->mask);
			builder->picture->mask = None;
		}
		if (!builder->have_alpha && builder->picture->alpha != None)
		{
			XFreePixmap(builder->dpy, builder->picture->alpha);
			builder->picture->alpha = None;
		}
		if (!builder->cancelled && !builder->argb32 &&
		    builder->effects == NULL &&
		    !(builder->picture->fpa_mask & FPAM_MONOCHROME))
		{
			builder->picture->average_valid = True;
			builder->picture->average_has_pixels =
			    picture_average_finish(&builder->average,
			    builder->picture->average_rgb);
		}
		builder->done = True;
	}
	return builder->done;
}

void picture_argb_cancel(picture_argb_builder *builder)
{
	builder->cancelled = True;
}

void picture_argb_creation_error(picture_argb_builder *builder,
	const XErrorEvent *error)
{
	builder->cancelled = True;
	if (error->request_code != X_CreatePixmap || builder->picture == NULL)
	{
		return;
	}
	Pixmap *pixmaps[] = {&builder->picture->picture,
	    &builder->picture->mask, &builder->picture->alpha};

	for (unsigned i = 0; i < 3; i++)
	{
		if (error->serial == builder->pixmap_serials[i])
		{
			*pixmaps[i] = None;
		}
	}
}

FvwmPicture *picture_argb_take(picture_argb_builder *builder)
{
	if (!builder->done || builder->cancelled)
	{
		errno = builder->cancelled ? ECANCELED : EBUSY;
		return NULL;
	}
	FvwmPicture *picture = builder->picture;

	builder->picture = NULL;
	return picture;
}

Bool picture_argb_destroy(picture_argb_builder *builder)
{
	if (builder == NULL)
	{
		return True;
	}
	if (!builder->done)
	{
		errno = EBUSY;
		return False;
	}
	XImage *images[] = {builder->color, builder->mask, builder->alpha};
	GC contexts[] = {builder->color_gc, builder->mask_gc,
	    builder->alpha_gc};

	for (unsigned i = 0; i < 3; i++)
	{
		if (images[i] != NULL)
		{
			XDestroyImage(images[i]);
		}
		if (contexts[i] != None)
		{
			XFreeGC(builder->dpy, contexts[i]);
		}
	}
	PDestroyFvwmPicture(builder->dpy, builder->picture);
	free(builder);
	return True;
}
