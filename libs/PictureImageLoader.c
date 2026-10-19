/* -*-c-*- */
/* Copyright (C) 1993, Robert Nation
 * Copyright (C) 1999 Carsten Haitzler and various contributors (imlib2)
 * Copyright (C) 2002  Olivier Chapuis */
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
 *
 * The png loader and PImageRGBtoPixel are from imlib2. The code is from raster
 * (Carsten Haitzler) <raster@rasterman.com> <raster@valinux.com>
 *
 */

/* ---------------------------- included header files ---------------------- */

#include "config.h"
#include "picture_average.h"

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <signal.h>
#include <ctype.h>
#include <setjmp.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xmd.h>

#include "fvwmlib.h"
#include "defaults.h"
#include "log.h"
#include "System.h"
#include "Strings.h"
#include "Picture.h"
#include "PictureUtils.h"
#include "Graphics.h"
#include "ColorUtils.h"
#include "Fxpm.h"
#include "Fpng.h"
#include "Fsvg.h"
#include "FRenderInit.h"
#include "Fcursor.h"
#include "FImage.h"
#include "ImageSafety.h"
#include "image_input.h"
#include "image_decompress.h"
#include "image_xbm.h"
#include "image_svg_client.h"
#include "fvwmsignal.h"
#include "libs/monitor_scale.h"
#include "libs/wm_ui_metrics.h"
#include "libs/picture_types.h"
/* ---------------------------- local definitions -------------------------- */
typedef struct
{
	image_input raw;
	image_input expanded;
	struct stat metadata;
	const image_input *xpm_input;
	char *path;
	char *allocated_path;
	Bool svg_options;
	Bool xpm_only;
	Bool xpm_checked;
	Bool defer_svg;
	Bool svg_deferred;
	Bool raw_borrowed;
	int xpm_hotspot[2];
} pimage_source;

#define FIMAGE_CMD_ARGS \
	Display *dpy, char *path, pimage_source *source, \
	CARD32 **argb_data, int *width, int *height, \
	monitor_scale_factor factor, unsigned target_width, \
	unsigned target_height

#define FIMAGE_PASS_ARGS \
	dpy, path, source, argb_data, width, height, factor, target_width, \
	target_height

typedef struct PImageLoader
{
  char *extension;
  int (*func)(FIMAGE_CMD_ARGS);
} PImageLoader;

/* ---------------------------- local macros ------------------------------- */

/* ---------------------------- imports ------------------------------------ */

/* ---------------------------- included code files ------------------------ */

/* ---------------------------- local types -------------------------------- */

/* ---------------------------- forward declarations ----------------------- */

static Bool PImageLoadSvg(FIMAGE_CMD_ARGS);
static Bool PImageLoadPng(FIMAGE_CMD_ARGS);
static Bool PImageLoadXpm(FIMAGE_CMD_ARGS);

/* ---------------------------- local variables ---------------------------- */

PImageLoader Loaders[] =
{
	{ "xpm", PImageLoadXpm },
	{ "svg", PImageLoadSvg },
	{ "png", PImageLoadPng },
	{NULL,0}
};

#define PIMAGE_MAX_DIMENSION 32768U
#define PIMAGE_MAX_PIXELS (64U * 1024U * 1024U)

/* ---------------------------- exported variables (globals) --------------- */

/* ---------------------------- local functions ---------------------------- */

static void
pimage_source_destroy(pimage_source *source)
{
	if (!source->raw_borrowed)
	{
		image_input_destroy(&source->raw);
	}
	image_input_destroy(&source->expanded);
	free(source->allocated_path);
}

static Bool
pimage_source_open(char *path, pimage_source *source)
{
	char *options_end;

	memset(source, 0, sizeof(*source));
	if (path == NULL)
	{
		errno = EINVAL;
		return False;
	}
	source->path = path;
	if (USE_SVG && path[0] == ':' &&
	    (options_end = strchr(path + 1, ':')) != NULL)
	{
		source->path = options_end + 1;
		source->svg_options = True;
	}
	if (image_input_load(source->path, FVWM_IMAGE_INPUT_LIMIT,
	    &source->raw, &source->metadata))
	{
		return True;
	}
	/* libXpm may try .Z/.gz when a bare filename is missing. Keep that
	 * fallback checked and XPM-only; never substitute for a refused inode. */
	if (errno == ENOENT && XpmSupport && !source->svg_options)
	{
		size_t length = strlen(path);

		if (length > SIZE_MAX - 4)
		{
			errno = EOVERFLOW;
			return False;
		}
		source->allocated_path = fxmalloc(length + 4);
		source->path = source->allocated_path;
		memcpy(source->path, path, length);
		memcpy(source->path + length, ".Z", 3);
		if (image_input_load(source->path, FVWM_IMAGE_INPUT_LIMIT,
		    &source->raw, &source->metadata))
		{
			source->xpm_only = True;
			return True;
		}
		if (errno == ENOENT)
		{
			memcpy(source->path + length, ".gz", 4);
			if (image_input_load(source->path, FVWM_IMAGE_INPUT_LIMIT,
			    &source->raw, &source->metadata))
			{
				source->xpm_only = True;
				return True;
			}
		}
	}
	fvwm_debug(__func__, "image source refused: %s", source->path);
	return False;
}

static bool
pimage_decompress_cancel(void *context)
{
	(void)context;
	return isTerminated != 0;
}

static const image_input *
pimage_xpm_source(pimage_source *source)
{
	if (source->svg_options || !XpmSupport)
	{
		return NULL;
	}
	if (!source->xpm_checked)
	{
		const char *extension = strrchr(source->path, '.');

		source->xpm_checked = True;
		if (extension != NULL && (strcmp(extension, ".Z") == 0 ||
		    strcmp(extension, ".gz") == 0))
		{
			char *args[] = {FVWM_IMAGE_GZIP, "-dqc", NULL};

			if (image_decompress(&source->raw, FVWM_IMAGE_GZIP,
			    args, FVWM_IMAGE_XPM_EXPANDED_LIMIT,
			    FVWM_IMAGE_DECOMPRESS_TIMEOUT_MS,
			    pimage_decompress_cancel, NULL, &source->expanded))
			{
				source->xpm_input = &source->expanded;
			}
			else
			{
				fvwm_debug(__func__, "compressed XPM input refused");
			}
		}
		else
		{
			source->xpm_input = &source->raw;
		}
	}
	return source->xpm_input;
}

static int
pimage_xpm_read(pimage_source *source, FxpmImage *image, FxpmInfo *info)
{
	const image_input *input = pimage_xpm_source(source);

	if (input == NULL || input->bytes == NULL || input->length == 0)
	{
		return FxpmOpenFailed;
	}
	return FxpmCreateXpmImageFromBuffer((char *)input->bytes, image, info);
}

static Bool
pimage_bitmap(Display *display, Window window, pimage_source *source,
	int *width, int *height, Pixmap *pixmap)
{
	unsigned char *bits;
	unsigned w, h;
	int hx, hy;

	if (source->svg_options || source->xpm_only ||
	    !image_xbm_read(&source->raw, INT_MAX, SIZE_MAX,
		&bits, &w, &h, &hx, &hy))
	{
		return False;
	}
	*pixmap = XCreateBitmapFromData(display, window, (char *)bits, w, h);
	free(bits);
	if (*pixmap == None)
	{
		return False;
	}
	*width = (int)w;
	*height = (int)h;
	return True;
}


static Bool
pimage_resolve_destination(
	int src_w, int src_h, unsigned target_width, unsigned target_height,
	monitor_scale_factor factor, int *dst_w, int *dst_h)
{
	if (dst_w == NULL || dst_h == NULL || src_w <= 0 || src_h <= 0)
	{
		return False;
	}
	if (target_width > 0 && target_height > 0)
	{
		if (target_width > (unsigned)INT_MAX ||
		    target_height > (unsigned)INT_MAX)
		{
			return False;
		}
		*dst_w = (int)target_width;
		*dst_h = (int)target_height;
		return True;
	}
	if (wm_ui_scale_px(src_w, factor, dst_w) !=
		MONITOR_SCALE_CONVERSION_OK ||
	    wm_ui_scale_px(src_h, factor, dst_h) !=
		MONITOR_SCALE_CONVERSION_OK ||
	    *dst_w <= 0 || *dst_h <= 0)
	{
		return False;
	}

	return True;
}

static Bool
pimage_blit_argb_once(
	CARD32 *src, int src_w, int src_h, int dst_w, int dst_h,
	CARD32 **dst_out)
{
	CARD32 *dst;
	size_t pixel_count;
	int y;
	int x;

	if (src == NULL || dst_out == NULL || src_w <= 0 || src_h <= 0 ||
	    dst_w <= 0 || dst_h <= 0)
	{
		return False;
	}
	if (!FImageSizeIsSafe(
		    (uint64_t)dst_w, (uint64_t)dst_h, PIMAGE_MAX_DIMENSION,
		    PIMAGE_MAX_PIXELS, &pixel_count))
	{
		return False;
	}
	if (src_w == dst_w && src_h == dst_h)
	{
		*dst_out = src;
		return True;
	}
	dst = fxcalloc(pixel_count, sizeof(CARD32));
	for (y = 0; y < dst_h; y++)
	{
		int src_y = (int)(((int64_t)y * src_h) / dst_h);

		if (src_y >= src_h)
		{
			src_y = src_h - 1;
		}
		for (x = 0; x < dst_w; x++)
		{
			int src_x = (int)(((int64_t)x * src_w) / dst_w);

			if (src_x >= src_w)
			{
				src_x = src_w - 1;
			}
			dst[(size_t)y * (size_t)dst_w + (size_t)x] =
				src[(size_t)src_y * (size_t)src_w +
				    (size_t)src_x];
		}
	}
	*dst_out = dst;

	return True;
}

static
Bool PImageLoadArgbDataFromInput(FIMAGE_CMD_ARGS)
{
	int done = 0, i = 0, tried = -1;
	char *ext = NULL;

	*argb_data = NULL;
	*width = *height = 0;
	if (path == NULL)
		return False;

	if (strlen(path) > 3)
	{
		ext = path + strlen(path) - 3;
	}
	/* first try to load by extension */
	while (!done && ext != NULL && Loaders[i].extension != NULL)
	{
		if (StrEquals(Loaders[i].extension, ext))
		{
			if (Loaders[i].func(FIMAGE_PASS_ARGS))
			{
				return True;
			}
			tried = i;
			done = 1;
		}
		i++;
	}

	i = 0;
	while (Loaders[i].extension != NULL)
	{
		if (i != tried && Loaders[i].func(FIMAGE_PASS_ARGS))
		{
			return True;
		}
		i++;
	}

	return False;
}

/*
 *
 * svg loader
 *
 */
static Bool
PImageLoadSvg(FIMAGE_CMD_ARGS)
{
#if USE_SVG
	if (source->xpm_only)
	{
		return False;
	}
	if (source->defer_svg)
	{
		/* Preserve the cheap PNG rejection and non-SVG fallbacks. The
		 * caller queues an SVG attempt only if no other format succeeds. */
		source->svg_deferred = !(source->raw.length >= 4 &&
		    memcmp(source->raw.bytes, "\211PNG", 4) == 0);
		return False;
	}
	return image_svg_load(&source->raw, path, factor, target_width,
	    target_height, argb_data, width, height);
#else
	return False;
#endif
}

/*
 *
 * png loader
 *
 */
#if PngSupport
typedef struct
{
	jmp_buf jump;
	CARD32 *data;
	unsigned char **lines;
	const image_input *input;
	size_t position;
} pimage_png_read;

static void pimage_png_error(png_structp png, png_const_charp message)
{
	pimage_png_read *read = png_get_error_ptr(png);

	/* Image contents are not diagnostic text. The heap-owned buffers remain
	 * defined after longjmp, including failures during read_end. */
	(void)message;
	fvwm_debug(__func__, "PNG decode failed");
	longjmp(read->jump, 1);
}

static void
pimage_png_read_data(png_structp png, png_bytep bytes, png_size_t count)
{
	pimage_png_read *read = png_get_io_ptr(png);

	if (count > read->input->length - read->position)
	{
		png_error(png, "short image input");
	}
	memcpy(bytes, read->input->bytes + read->position, count);
	read->position += count;
}

static
Bool PImageLoadPng(FIMAGE_CMD_ARGS)
{
	Fpng_uint_32 w32, h32;
	Fpng_structp Fpng_ptr = NULL;
	Fpng_infop Finfo_ptr = NULL;
	CARD32 *data;
	int w, h;
	char hasa, hasg;
	int bit_depth;
	int color_type;
	int interlace_type;
	unsigned char **lines;
	pimage_png_read *read;
	int i;
	size_t pixel_count;
	int dst_w;
	int dst_h;
	CARD32 *scaled;

	if (source->svg_options || source->xpm_only ||
	    source->raw.length < FPNG_BYTES_TO_CHECK ||
	    !Fpng_check_sig(source->raw.bytes, FPNG_BYTES_TO_CHECK))
	{
		return False;
	}
	read = fxcalloc(1, sizeof(*read));
	read->input = &source->raw;
	Fpng_ptr = Fpng_create_read_struct(FPNG_LIBPNG_VER_STRING,
					   NULL, NULL, NULL);

	if (!Fpng_ptr)
	{
		free(read);
		return False;
	}
	Finfo_ptr = Fpng_create_info_struct(Fpng_ptr);
	if (!Finfo_ptr)
	{
		Fpng_destroy_read_struct(&Fpng_ptr, NULL, NULL);
		free(read);
		return False;
	}
	if (setjmp(read->jump))
	{
		free(read->lines);
		free(read->data);
		Fpng_destroy_read_struct(&Fpng_ptr, &Finfo_ptr, NULL);
		free(read);
		return False;
	}
	/* Construction uses libpng's own recovery. Install our callback only
	 * after its destination has been armed. */
	png_set_error_fn(Fpng_ptr, read, pimage_png_error, NULL);
	png_set_read_fn(Fpng_ptr, read, pimage_png_read_data);
	Fpng_read_info(Fpng_ptr, Finfo_ptr);
	Fpng_get_IHDR(
		Fpng_ptr, Finfo_ptr, (Fpng_uint_32 *) (&w32),
		(Fpng_uint_32 *) (&h32), &bit_depth, &color_type,
		&interlace_type, NULL, NULL);
	if (w32 > (Fpng_uint_32)INT_MAX || h32 > (Fpng_uint_32)INT_MAX ||
	    !FImageSizeIsSafe(
		    w32, h32, PIMAGE_MAX_DIMENSION, PIMAGE_MAX_PIXELS,
		    &pixel_count))
	{
		Fpng_destroy_read_struct(&Fpng_ptr, &Finfo_ptr, NULL);
		free(read);
		return False;
	}
	interlace_type = 0; /* not used */
	w = (int) w32;
	h = (int) h32;
	if (color_type == FPNG_COLOR_TYPE_PALETTE)
	{
		Fpng_set_expand(Fpng_ptr);
	}

	hasa = color_type == FPNG_COLOR_TYPE_RGB_ALPHA ||
		color_type == FPNG_COLOR_TYPE_GRAY_ALPHA;
	hasg = color_type == FPNG_COLOR_TYPE_GRAY ||
		color_type == FPNG_COLOR_TYPE_GRAY_ALPHA;

	if (hasa)
		Fpng_set_expand(Fpng_ptr);
	/* we want ARGB */
	/* note form raster:
	* thanks to mustapha for helping debug this on PPC Linux remotely by
	* sending across screenshots all the time and me figuring out form them
	* what the hell was up with the colors
	* now png loading should work on big endian machines nicely   */
#ifdef WORDS_BIGENDIAN
	Fpng_set_swap_alpha(Fpng_ptr);
	Fpng_set_filler(Fpng_ptr, 0xff, FPNG_FILLER_BEFORE);
#else
	Fpng_set_bgr(Fpng_ptr);
	Fpng_set_filler(Fpng_ptr, 0xff, FPNG_FILLER_AFTER);
#endif
	/* 16bit color -> 8bit color */
	Fpng_set_strip_16(Fpng_ptr);
	/* pack all pixels to byte boundaires */
	Fpng_set_packing(Fpng_ptr);
	if (Fpng_get_valid(Fpng_ptr, Finfo_ptr, FPNG_INFO_tRNS))
	{
		Fpng_set_expand(Fpng_ptr);
	}

	data = fxcalloc(pixel_count, sizeof(CARD32));
	lines = fxcalloc((size_t)h, sizeof(unsigned char *));
	read->data = data;
	read->lines = lines;

	if (hasg)
	{
		Fpng_set_gray_to_rgb(Fpng_ptr);
		if (Fpng_get_bit_depth(Fpng_ptr, Finfo_ptr) < 8)
		{
			Fpng_set_gray_1_2_4_to_8(Fpng_ptr);
		}
	}
	for (i = 0; i < h; i++)
	{
		lines[i] = (unsigned char *)data + (i * w * sizeof(CARD32));
	}
	Fpng_read_image(Fpng_ptr, lines);
	Fpng_read_end(Fpng_ptr, Finfo_ptr);
	Fpng_destroy_read_struct(&Fpng_ptr, &Finfo_ptr, (png_infopp) NULL);
	free(read);
	free(lines);
	if (!pimage_resolve_destination(
		    w, h, target_width, target_height, factor, &dst_w,
		    &dst_h) ||
	    !pimage_blit_argb_once(data, w, h, dst_w, dst_h, &scaled))
	{
		free(data);
		return False;
	}
	if (scaled != data)
	{
		free(data);
	}
	*width = dst_w;
	*height = dst_h;
	*argb_data = scaled;

	return True;
}

#else
static Bool PImageLoadPng(FIMAGE_CMD_ARGS)
{
	fvwm_debug(__func__, "PNG support is disabled");
	return False;
}
#endif

/*
 *
 * xpm loader
 *
 */
static
Bool PImageLoadXpm(FIMAGE_CMD_ARGS)
{
	FxpmImage xpm_im;
	FxpmInfo xpm_info;
	FxpmColor *xpm_color;
	XColor color;
	CARD32 *colors;
	CARD32 *data;
	char *visual_color;
	int i;
	int w;
	int h;
	int rc;
	size_t pixel_count;
	size_t pixel_index;
	int dst_w;
	int dst_h;
	CARD32 *scaled;

	if (!XpmSupport)
	{
		fvwm_debug(__func__,
				"[PImageLoadXpm]: Tried to load XPM file "
				"when FVWM has not been compiled with XPm Support.\n");
		return False;
	}
	memset(&xpm_im, 0, sizeof(FxpmImage));

	memset(&xpm_info, 0, sizeof(xpm_info));
	source->xpm_hotspot[0] = source->xpm_hotspot[1] = -1;
	rc = pimage_xpm_read(source, &xpm_im, &xpm_info);
	if (rc == FxpmSuccess && (xpm_info.valuemask & FxpmHotspot))
	{
		source->xpm_hotspot[0] = xpm_info.x_hotspot;
		source->xpm_hotspot[1] = xpm_info.y_hotspot;
	}
	FxpmFreeXpmInfo(&xpm_info);

	if (rc != FxpmSuccess)
	{
		return False;
	}

	if (xpm_im.ncolors == 0 || xpm_im.ncolors > INT_MAX ||
	    xpm_im.colorTable == NULL || xpm_im.data == NULL)
	{
		FxpmFreeXpmImage(&xpm_im);
		return False;
	}
	if (!FImageSizeIsSafe(
		    xpm_im.width, xpm_im.height, PIMAGE_MAX_DIMENSION,
		    PIMAGE_MAX_PIXELS, &pixel_count))
	{
		FxpmFreeXpmImage(&xpm_im);
		return False;
	}
	colors = fxmalloc(xpm_im.ncolors * sizeof(CARD32));
	for (i=0; i < xpm_im.ncolors; i++)
	{
		xpm_color = &xpm_im.colorTable[i];
		if (xpm_color->c_color)
		{
			visual_color = xpm_color->c_color;
		}
		else if (xpm_color->g_color)
		{
			visual_color = xpm_color->g_color;
		}
		else if (xpm_color->g4_color)
		{
			visual_color = xpm_color->g4_color;
		}
		else
		{
			visual_color = xpm_color->m_color;
		}
		if (visual_color != NULL &&
		    XParseColor(dpy, Pcmap, visual_color, &color))
		{
			colors[i] = 0xff000000 |
				((color.red  << 8) & 0xff0000) |
				((color.green    ) & 0xff00) |
				((color.blue >> 8) & 0xff);
		}
		else
		{
			colors[i] = 0;
		}
	}
	w = xpm_im.width;
	h = xpm_im.height;
	data = fxcalloc(pixel_count, sizeof(CARD32));
	for (pixel_index = 0; pixel_index < pixel_count; pixel_index++)
	{
		if (xpm_im.data[pixel_index] >= xpm_im.ncolors)
		{
			free(colors);
			free(data);
			FxpmFreeXpmImage(&xpm_im);
			return False;
		}
		data[pixel_index] = colors[xpm_im.data[pixel_index]];
	}
	free(colors);
	FxpmFreeXpmImage(&xpm_im);
	if (!pimage_resolve_destination(
		    w, h, target_width, target_height, factor, &dst_w,
		    &dst_h) ||
	    !pimage_blit_argb_once(data, w, h, dst_w, dst_h, &scaled))
	{
		free(data);
		return False;
	}
	if (scaled != data)
	{
		free(data);
	}
	*width = dst_w;
	*height = dst_h;
	*argb_data = scaled;

	return True;
}

/*
 *
 * copy image to server
 *
 */
static
Pixmap PImageCreatePixmapFromFImage(Display *dpy, Window win, FImage *fimage)
{
	GC gc;
	Pixmap pixmap;
	int w;
	int h;
	int depth;

	w = fimage->im->width;
	h = fimage->im->height;
	depth = fimage->im->depth;
	pixmap = XCreatePixmap(dpy, win, w, h, depth);
	if (depth == Pdepth)
	{
		gc = PictureDefaultGC(dpy, win);
	}
	else
	{
		gc = fvwmlib_XCreateGC(dpy, pixmap, 0, NULL);
	}
	FPutFImage(dpy, pixmap, gc, fimage, 0, 0, 0, 0, w, h);
	if (depth != Pdepth)
	{
		XFreeGC(dpy, gc);
	}

	return pixmap;
}

/* ---------------------------- interface functions ------------------------ */

/*
 *
 * argb data to pixmaps
 *
 */
static Bool pimage_create_pixmap_from_argb(
	Display *dpy, Window win, CARD32 *data, size_t data_len, int start,
	int width, int height, Pixmap *pixmap, Pixmap *mask, Pixmap *alpha,
	int *nalloc_pixels, Pixel **alloc_pixels, int *no_limit,
	FvwmPictureAttributes fpa, picture_average *average)
{
	FImage *fim;
	FImage *m_fim = NULL;
	FImage *a_fim = NULL;
	XColor c;
	int i;
	int j;
	int a;
	PictureImageColorAllocator *pica = NULL;
	int alpha_limit = PICTURE_ALPHA_LIMIT;
	int alpha_depth = FRenderGetAlphaDepth();
	Bool have_mask = False;
	Bool have_alpha = False;
	size_t pixel_count;

	if (data == NULL || start < 0 || width <= 0 || height <= 0 ||
	    !FImageSizeIsSafe(
		    (uint64_t)width, (uint64_t)height, PIMAGE_MAX_DIMENSION,
		    PIMAGE_MAX_PIXELS, &pixel_count) ||
	    (size_t)start > data_len || pixel_count > data_len - (size_t)start)
	{
		return False;
	}

	fim = FCreateFImage(
		dpy, Pvisual, (fpa.mask & FPAM_MONOCHROME) ? 1 : Pdepth,
		ZPixmap, width, height);
	if (!fim)
	{
		return False;
	}
	if (mask)
	{
		m_fim = FCreateFImage(
			dpy, Pvisual, 1, ZPixmap, width, height);
	}
	if (alpha && !(fpa.mask & FPAM_NO_ALPHA) && alpha_depth)
	{
		alpha_limit = 0;
		a_fim = FCreateFImage(
			dpy, Pvisual, alpha_depth, ZPixmap, width, height);
	}
	if (!(fpa.mask & FPAM_MONOCHROME))
	{
		c.flags = DoRed | DoGreen | DoBlue;
		pica = PictureOpenImageColorAllocator(
			dpy, Pcmap, width, height,
			!!(fpa.mask & FPAM_NO_COLOR_LIMIT),
			!!(fpa.mask & FPAM_NO_ALLOC_PIXELS),
			!!(fpa.mask & FPAM_DITHER),
			True);
	}
	data += start;
	for (j = 0; j < height; j++)
	{
		for (i = 0; i < width; i++, data++)
		{
			if (average != NULL &&
			    !picture_average_add(average, *data))
			{
				goto average_failed;
			}
			a = (*data >> 030) & 0xff;
			if (a > alpha_limit)
			{
				c.red   = (*data >> 16) & 0xff;
				c.green = (*data >>  8) & 0xff;
				c.blue  = (*data      ) & 0xff;
				if (pica)
				{
					PictureAllocColorImage(
						dpy, pica, &c, i, j);
					XPutPixel(fim->im, i, j, c.pixel);
				}
				/* Brightness threshold */
				else if ((0x99  * c.red +
					  0x12D * c.green +
					  0x3A  * c.blue) >> 16)
				{
					XPutPixel(fim->im, i, j, 1);
				}
				else
				{
					XPutPixel(fim->im, i, j, 0);
				}
				if (m_fim)
				{
					XPutPixel(m_fim->im, i, j, 1);
				}
			}
			else if (m_fim != NULL)
			{
				XPutPixel(m_fim->im, i, j, 0);
				have_mask = True;
			}
			if (a_fim != NULL)
			{
				XPutPixel(a_fim->im, i, j, a);
				if (a > 0 && a < 0xff)
				{
					have_alpha = True;
				}
			}
		}
	}
	if (pica)
	{
		PictureCloseImageColorAllocator(
			dpy, pica, nalloc_pixels, alloc_pixels, no_limit);
	}
	*pixmap = PImageCreatePixmapFromFImage(dpy, win, fim);
	if (have_alpha)
	{
		*alpha = PImageCreatePixmapFromFImage(dpy, win, a_fim);
	}
	else if (have_mask)
	{
		*mask = PImageCreatePixmapFromFImage(dpy, win, m_fim);
	}
	FDestroyFImage(dpy, fim);
	if (m_fim)
	{
		FDestroyFImage(dpy, m_fim);
	}
	if (a_fim)
	{
		FDestroyFImage(dpy, a_fim);
	}

	return True;

average_failed:
	/* The decoded size is already bounded, so this is an internal state
	 * failure. Still release colors/images before refusing the picture. */
	if (pica != NULL)
	{
		int allocated = 0, unlimited = 0;
		Pixel *pixels = NULL;

		PictureCloseImageColorAllocator(dpy, pica, &allocated,
		    &pixels, &unlimited);
		if (allocated > 0)
		{
			PictureFreeColors(dpy, Pcmap, pixels, allocated, 0,
			    unlimited);
		}
		free(pixels);
	}
	FDestroyFImage(dpy, fim);
	if (m_fim != NULL)
	{
		FDestroyFImage(dpy, m_fim);
	}
	if (a_fim != NULL)
	{
		FDestroyFImage(dpy, a_fim);
	}
	errno = EOVERFLOW;
	return False;
}

Bool PImageCreatePixmapFromArgbData(
	Display *dpy, Window win, CARD32 *data, size_t data_len, int start,
	int width, int height, Pixmap *pixmap, Pixmap *mask, Pixmap *alpha,
	int *nalloc_pixels, Pixel **alloc_pixels, int *no_limit,
	FvwmPictureAttributes fpa)
{
	return pimage_create_pixmap_from_argb(dpy, win, data, data_len,
	    start, width, height, pixmap, mask, alpha, nalloc_pixels,
	    alloc_pixels, no_limit, fpa, NULL);
}


/*
 *
 * the images loaders
 *
 */

static void
pimage_reset_pixmap_outputs(Pixmap *pixmap, Pixmap *mask, Pixmap *alpha,
	int *width, int *height, int *depth, int *nalloc_pixels,
	Pixel **alloc_pixels, int *no_limit)
{
	if (pixmap != NULL)
	{
		*pixmap = None;
	}
	if (mask != NULL)
	{
		*mask = None;
	}
	if (alpha != NULL)
	{
		*alpha = None;
	}
	*width = *height = *depth = 0;
	if (nalloc_pixels != NULL)
	{
		*nalloc_pixels = 0;
	}
	if (alloc_pixels != NULL)
	{
		*alloc_pixels = NULL;
	}
	if (no_limit != NULL)
	{
		*no_limit = False;
	}
}

Bool PImageLoadPixmapFromFile(
	Display *dpy, Window win, char *path, Pixmap *pixmap, Pixmap *mask,
	Pixmap *alpha, int *width, int *height, int *depth,
	int *nalloc_pixels, Pixel **alloc_pixels,
	int *no_limit, FvwmPictureAttributes fpa)
{
	CARD32 *data = NULL;
	pimage_source source = {0};
	Bool success = False;
	monitor_scale_factor factor = {MONITOR_SCALE_QUARTERS_MIN};

	pimage_reset_pixmap_outputs(pixmap, mask, alpha, width, height, depth,
	    nalloc_pixels, alloc_pixels, no_limit);
	if (!pimage_source_open(path, &source))
	{
		goto done;
	}
	if (PImageLoadArgbDataFromInput(
		    dpy, path, &source, &data, width, height, factor, 0, 0))
	{
		size_t pixel_count;

		if (*width <= 0 || *height <= 0 ||
		    !FImageSizeIsSafe(
			    (uint64_t)*width, (uint64_t)*height,
			    PIMAGE_MAX_DIMENSION, PIMAGE_MAX_PIXELS, &pixel_count))
		{
			goto done;
		}
		*depth = (fpa.mask & FPAM_MONOCHROME) ? 1 : Pdepth;
		success = PImageCreatePixmapFromArgbData(
		    dpy, win, data, pixel_count, 0, *width, *height, pixmap, mask,
		    alpha, nalloc_pixels, alloc_pixels, no_limit, fpa);
	}
	else if (pimage_bitmap(dpy, win, &source, width, height, pixmap))
	{
		*depth = 1;
		success = True;
	}

done:
	free(data);
	pimage_source_destroy(&source);
	if (!success)
	{
		pimage_reset_pixmap_outputs(pixmap, mask, alpha, width, height,
		    depth, nalloc_pixels, alloc_pixels, no_limit);
	}
	return success;
}

static FvwmPicture *picture_load_file(
	Display *dpy, Window win, char *path, FvwmPictureAttributes fpa,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height, Bool defer_svg, Bool *svg_deferred,
	const image_input *input, const struct stat *metadata,
	const image_input *xpm_input, Bool xpm_only)
{
	FvwmPicture *p = NULL;
	Pixmap pixmap = None;
	Pixmap mask = None;
	Pixmap alpha = None;
	int width = 0, height = 0;
	int depth = 0, no_limit = 0;
	int nalloc_pixels = 0;
	Pixel *alloc_pixels = NULL;
	CARD32 *data = NULL;
	pimage_source source = {0};
	picture_average average = {0};
	size_t pixel_count;

	if (input != NULL)
	{
		if (metadata == NULL || input->length > FVWM_IMAGE_INPUT_LIMIT ||
		    (input->length != 0 && input->bytes == NULL))
		{
			errno = EINVAL;
			return NULL;
		}
		source.raw = *input;
		source.raw_borrowed = True;
		source.metadata = *metadata;
		source.path = path;
		char *options_end;

		if (USE_SVG && path[0] == ':' &&
		    (options_end = strchr(path + 1, ':')) != NULL)
		{
			source.path = options_end + 1;
			source.svg_options = True;
		}
	}
	else if (!pimage_source_open(path, &source))
	{
		goto done;
	}
	if (input != NULL)
	{
		/* The active owner has already attempted expansion. A NULL XPM
		 * snapshot records refusal and must never trigger another wait. */
		source.xpm_checked = True;
		source.xpm_input = xpm_input;
		source.xpm_only = xpm_only;
	}
	source.defer_svg = defer_svg;
	if (PImageLoadArgbDataFromInput(
		    dpy, path, &source, &data, &width, &height, factor,
		    target_width, target_height))
	{
		if (width <= 0 || height <= 0 ||
		    !FImageSizeIsSafe(
			    (uint64_t)width, (uint64_t)height,
			    PIMAGE_MAX_DIMENSION, PIMAGE_MAX_PIXELS,
			    &pixel_count))
		{
			goto done;
		}
		depth = (fpa.mask & FPAM_MONOCHROME) ? 1 : Pdepth;
		if (!pimage_create_pixmap_from_argb(
			    dpy, win, data, pixel_count, 0, width, height,
			    &pixmap, &mask, &alpha, &nalloc_pixels,
			    &alloc_pixels, &no_limit, fpa,
			    depth == 1 ? NULL : &average))
		{
			goto done;
		}
	}
	else if (pimage_bitmap(dpy, win, &source, &width, &height, &pixmap))
	{
		depth = 1;
	}
	else
	{
		goto done;
	}

	p = fxcalloc(1, sizeof(FvwmPicture));
	p->count = 1;
	p->name = path;
	p->fpa_mask = fpa.mask;
	p->next = NULL;
	/* Keep the existing cache stamp formula, using the opened image's
	 * metadata rather than stamping a possibly replaced pathname. */
	p->stamp = ((FileStamp)source.metadata.st_mtime << 13) +
	    (FileStamp)source.metadata.st_size;
	p->picture = pixmap;
	p->mask = mask;
	p->alpha = alpha;
	p->width = width;
	p->height = height;
	p->depth = depth;
	p->nalloc_pixels = nalloc_pixels;
	p->alloc_pixels = alloc_pixels;
	p->no_limit = no_limit;
	p->color_cmap = Pcmap;
	if (depth != 1)
	{
		p->average_valid = True;
		p->average_has_pixels = picture_average_finish(&average,
		    p->average_rgb);
	}

done:
	if (svg_deferred != NULL)
	{
		*svg_deferred = source.svg_deferred;
	}
	free(data);
	pimage_source_destroy(&source);
	return p;
}

FvwmPicture *PImageLoadFvwmPictureFromFile(
	Display *dpy, Window win, char *path, FvwmPictureAttributes fpa,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height)
{
	return picture_load_file(dpy, win, path, fpa, factor, target_width,
	    target_height, False, NULL, NULL, NULL, NULL, False);
}

FvwmPicture *picture_load_non_svg(Display *dpy, Window win, char *path,
	FvwmPictureAttributes fpa, monitor_scale_factor factor,
	unsigned target_width, unsigned target_height, Bool *svg_deferred)
{
	if (svg_deferred == NULL)
	{
		errno = EINVAL;
		return NULL;
	}
	*svg_deferred = False;
	return picture_load_file(dpy, win, path, fpa, factor, target_width,
	    target_height, True, svg_deferred, NULL, NULL, NULL, False);
}

FvwmPicture *picture_load_non_svg_input(Display *dpy, Window win, char *path,
	const image_input *input, const struct stat *metadata,
	const image_input *xpm_input, FvwmPictureAttributes fpa,
	monitor_scale_factor factor,
	unsigned target_width, unsigned target_height)
{
	if (path == NULL || input == NULL)
	{
		errno = EINVAL;
		return NULL;
	}
	return picture_load_file(dpy, win, path, fpa, factor, target_width,
	    target_height, True, NULL, input, metadata, xpm_input, False);
}

Bool
picture_decode_xpm_cursor_input(Display *dpy, char *path,
	const image_input *input, const image_input *xpm_input,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height, CARD32 **pixels, int *width, int *height, int hotspot[2])
{
	pimage_source source = {0};

	*pixels = NULL;
	*width = *height = 0;
	if (input == NULL || path == NULL)
	{
		return False;
	}
	source.raw = *input;
	source.raw_borrowed = source.xpm_only = source.xpm_checked = True;
	source.path = path;
	source.xpm_input = xpm_input;
	if (USE_SVG && path[0] == ':' && strchr(path + 1, ':') != NULL)
	{
		source.svg_options = True;
	}
	Bool loaded = PImageLoadXpm(dpy, path, &source, pixels, width, height,
	    factor, target_width, target_height);

	if (hotspot != NULL)
	{
		hotspot[0] = loaded ? source.xpm_hotspot[0] : -1;
		hotspot[1] = loaded ? source.xpm_hotspot[1] : -1;
	}
	return loaded;
}

Bool picture_decode_xpm_input(Display *dpy, char *path,
	const image_input *input, const image_input *xpm_input,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height, CARD32 **pixels, int *width, int *height)
{
	return picture_decode_xpm_cursor_input(dpy, path, input, xpm_input,
	    factor, target_width, target_height, pixels, width, height, NULL);
}

static int
pimage_cursor_nominal_px(monitor_scale_factor factor)
{
	int nominal = WM_UI_XCURSOR_NOMINAL_SOURCE_PX;

	if (wm_ui_scale_px(WM_UI_XCURSOR_NOMINAL_SOURCE_PX, factor,
		&nominal) != MONITOR_SCALE_CONVERSION_OK)
	{
		fvwm_debug(__func__,
			"failed to scale cursor nominal %d; using 1x",
			WM_UI_XCURSOR_NOMINAL_SOURCE_PX);
		return WM_UI_XCURSOR_NOMINAL_SOURCE_PX;
	}

	return nominal;
}

static void
pimage_cursor_scale_hotspot(
	int source_hot, int extent, monitor_scale_factor factor, int *hot)
{
	if (wm_ui_scale_hotspot(source_hot, extent, factor, hot)
	    != MONITOR_SCALE_CONVERSION_OK)
	{
		fvwm_debug(__func__,
			"failed to scale cursor hotspot %d extent %d; using 1x",
			source_hot, extent);
		*hot = source_hot;
		if (*hot < 0)
		{
			*hot = 0;
		}
		else if (extent > 0 && *hot >= extent)
		{
			*hot = extent - 1;
		}
	}
}

#ifdef HAVE_XCURSOR
typedef struct
{
	const image_input *input;
	size_t position;
} pimage_cursor_input;

static int
pimage_cursor_read(XcursorFile *file, unsigned char *bytes, int count)
{
	pimage_cursor_input *input = file->closure;
	size_t available = input->input->length - input->position;

	if (count <= 0)
	{
		return 0;
	}
	if (available > (size_t)count)
	{
		available = (size_t)count;
	}
	memcpy(bytes, input->input->bytes + input->position, available);
	input->position += available;
	return (int)available;
}

static int
pimage_cursor_seek(XcursorFile *file, long offset, int whence)
{
	pimage_cursor_input *input = file->closure;
	size_t base;

	switch (whence)
	{
	case SEEK_SET:
		base = 0;
		break;
	case SEEK_CUR:
		base = input->position;
		break;
	case SEEK_END:
		base = input->input->length;
		break;
	default:
		return -1;
	}
	if (offset < 0)
	{
		uintmax_t magnitude = (uintmax_t)(-(offset + 1)) + 1;

		if (magnitude > base)
		{
			return -1;
		}
		input->position = base - (size_t)magnitude;
	}
	else
	{
		if ((uintmax_t)offset > input->input->length - base)
		{
			return -1;
		}
		input->position = base + (size_t)offset;
	}
	return 0;
}
#endif

static FcursorImages *
pimage_cursor_images(pimage_source *source, int nominal)
{
#ifdef HAVE_XCURSOR
	pimage_cursor_input input = {&source->raw, 0};
	XcursorFile file = {&input, pimage_cursor_read, NULL, pimage_cursor_seek};

	if (source->svg_options || source->xpm_only)
	{
		return NULL;
	}
	return XcursorXcFileLoadImages(&file, nominal);
#else
	return NULL;
#endif
}

Cursor PImageLoadCursorFromFile(
	Display *dpy, Window win, char *path, int x_hot, int y_hot,
	monitor_scale_factor factor)
{
	Cursor cursor = 0;
	pimage_source source = {0};
	CARD32 *data;
	int width;
	int height;
	int i;
	int nominal;
	FcursorImages *fcis;
	FcursorImage *fci;
	monitor_scale_factor one_x = {MONITOR_SCALE_QUARTERS_MIN};

	if (!pimage_source_open(path, &source))
	{
		pimage_source_destroy(&source);
		return 0;
	}
	nominal = pimage_cursor_nominal_px(factor);
	/* First try the Xcursor loader (animated cursors) */
	if ((fcis = pimage_cursor_images(&source, nominal)))
	{
		for (i = 0; i < fcis->nimage; i++)
		{
			int hx;
			int hy;
			int extent_w = fcis->images[i]->width;
			int extent_h = fcis->images[i]->height;

			if (x_hot >= 0)
			{
				pimage_cursor_scale_hotspot(
					x_hot, extent_w, factor, &hx);
			}
			else
			{
				pimage_cursor_scale_hotspot(
					fcis->images[i]->xhot, extent_w,
					one_x, &hx);
			}
			if (y_hot >= 0)
			{
				pimage_cursor_scale_hotspot(
					y_hot, extent_h, factor, &hy);
			}
			else
			{
				pimage_cursor_scale_hotspot(
					fcis->images[i]->yhot, extent_h,
					one_x, &hy);
			}
			fcis->images[i]->xhot = hx;
			fcis->images[i]->yhot = hy;
		}
		cursor = FcursorImagesLoadCursor(dpy, fcis);
		FcursorImagesDestroy(fcis);
	}
	/* Get cursor data from the regular image loader */
	else if (PImageLoadArgbDataFromInput(
		    dpy, path, &source, &data, &width, &height, factor, 0, 0))
	{
		Pixmap src;
		Pixmap msk = None;
		FvwmPictureAttributes fpa;

		fpa.mask = FPAM_NO_ALPHA | FPAM_MONOCHROME;

		/* Adjust the hot-spot if necessary */
		if (x_hot < 0 || y_hot < 0)
		{
			FxpmImage xpm_im;
			FxpmInfo xpm_info;

			memset(&xpm_im, 0, sizeof(FxpmImage));
			memset(&xpm_info, 0, sizeof(FxpmInfo));
			if (pimage_xpm_read(&source, &xpm_im, &xpm_info)
			    == FxpmSuccess)
			{
				if (xpm_info.valuemask & FxpmHotspot)
				{
					if (x_hot < 0)
					{
						x_hot = xpm_info.x_hotspot;
					}
					if (y_hot < 0)
					{
						y_hot = xpm_info.y_hotspot;
					}
				}
				FxpmFreeXpmImage(&xpm_im);
				FxpmFreeXpmInfo(&xpm_info);
			}
		}
		if (x_hot >= 0)
		{
			pimage_cursor_scale_hotspot(
				x_hot, width, factor, &x_hot);
		}
		else
		{
			x_hot = width / 2;
		}
		if (y_hot >= 0)
		{
			pimage_cursor_scale_hotspot(
				y_hot, height, factor, &y_hot);
		}
		else
		{
			y_hot = height / 2;
		}
		/* Use the Xcursor library to create the argb cursor */
		if ((fci = FcursorImageCreate(width, height)))
		{
			unsigned char alpha;
			unsigned char red;
			unsigned char green;
			unsigned char blue;

			/* Xcursor expects alpha prescaled RGB values */
			for (i = 0; i < width * height; i++)
			{
				alpha = ((data[i] >> 24) & 0xff);
				red   = ((data[i] >> 16) & 0xff) * alpha/0xff;
				green = ((data[i] >>  8) & 0xff) * alpha/0xff;
				blue  = ((data[i]      ) & 0xff) * alpha/0xff;

				data[i] =
					((CARD32)alpha << 24) |
					((CARD32)red << 16) |
					((CARD32)green << 8) | blue;
			}

			fci->xhot = x_hot;
			fci->yhot = y_hot;
			fci->delay = 0;
			/* Xcursor owns its allocated pixels; do not replace that
			 * pointer with data which this function also frees. */
			memcpy(fci->pixels, data,
			    (size_t)width * (size_t)height * sizeof(CARD32));
			cursor = FcursorImageLoadCursor(dpy, fci);
			FcursorImageDestroy(fci);
		}
		/* Create monochrome cursor from argb data */
		else if (PImageCreatePixmapFromArgbData(
			dpy, win, data, (size_t)width * (size_t)height, 0,
			width, height,
			&src, &msk, 0, 0, 0, 0, fpa))
		{
			XColor c[2];

			c[0].pixel = GetColor(DEFAULT_CURSOR_FORE_COLOR);
			c[1].pixel = GetColor(DEFAULT_CURSOR_BACK_COLOR);
			XQueryColors(dpy, Pcmap, c, 2);
			cursor = XCreatePixmapCursor(
				dpy, src, msk, &(c[0]), &(c[1]), x_hot, y_hot);
			XFreePixmap(dpy, src);
			if (msk != None)
			{
				XFreePixmap(dpy, msk);
			}
		}
		free(data);
	}

	pimage_source_destroy(&source);
	return cursor;
}

/* FIXME: Use color limit */
Bool PImageLoadPixmapFromXpmData(
	Display *dpy, Window win, int color_limit,
	char **data,
	Pixmap *pixmap, Pixmap *mask,
	int *width, int *height, int *depth)
{
	FxpmAttributes xpm_attributes;

	if (!XpmSupport)
	{
		return False;
	}
	xpm_attributes.valuemask = FxpmCloseness |
		FxpmExtensions | FxpmVisual | FxpmColormap | FxpmDepth;
	xpm_attributes.closeness = 40000;
	xpm_attributes.visual = Pvisual;
	xpm_attributes.colormap = Pcmap;
	xpm_attributes.depth = Pdepth;
	/* suppress compiler warning if xpm library is not compiled in */
	xpm_attributes.width = 0;
	xpm_attributes.height = 0;
	if (
		FxpmCreatePixmapFromData(
			dpy, win, data, pixmap, mask, &xpm_attributes) !=
		FxpmSuccess)
	{
		return False;
	}
	*width = xpm_attributes.width;
	*height = xpm_attributes.height;
	*depth = Pdepth;

	return True;
}
