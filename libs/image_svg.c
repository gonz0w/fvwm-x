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

#include "config.h"
#include "image_svg.h"
#include "Fsvg.h"
#include "ImageSafety.h"

#include <X11/Xmd.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIMAGE_MAX_DIMENSION 32768U
#define PIMAGE_MAX_PIXELS (64U * 1024U * 1024U)

#if USE_SVG
static FRsvgHandle *
image_svg_from_input(const image_input *source, const char *path)
{
	GFile *base = g_file_new_for_path(path);
	GInputStream *stream = g_memory_input_stream_new_from_data(
	    source->bytes, (gssize)source->length, NULL);
	FRsvgHandle *handle = rsvg_handle_new_from_stream_sync(
	    stream, base, RSVG_HANDLE_FLAGS_NONE, NULL, NULL);

	g_object_unref(stream);
	g_object_unref(base);
	return handle;
}

static bool
image_svg_dimension(double value, int *dimension)
{
	if (!isfinite(value) || value <= 0 || value > INT_MAX)
	{
		return false;
	}
	*dimension = (int)value;
	return *dimension > 0;
}

/* Match the legacy integer-pair forms without scanf integer overflow.
 * Unmatched characters retain the old skip-one-character behavior. */
static int
image_svg_pair(const char *text, int *first, int *second,
	int *second_start, int *consumed)
{
	char *end_first, *end_second;
	long a, b;
	bool first_valid, dimensions;

	errno = 0;
	a = strtol(text, &end_first, 10);
	if (end_first == text)
	{
		return 0;
	}
	first_valid = errno != ERANGE && a >= INT_MIN && a <= INT_MAX;
	dimensions = *end_first == 'x';
	const char *next = end_first + (dimensions ? 1 : 0);

	errno = 0;
	b = strtol(next, &end_second, 10);
	if (end_second == next)
	{
		return 0;
	}
	if (!first_valid || errno == ERANGE || b < INT_MIN || b > INT_MAX ||
	    end_second - text > INT_MAX)
	{
		return -1;
	}
	*first = (int)a;
	*second = (int)b;
	*second_start = (int)(next - text);
	*consumed = (int)(end_second - text);
	return dimensions ? 1 : 2;
}

bool
image_svg_render(const image_input *source, const char *filename,
	monitor_scale_factor factor, unsigned target_width, unsigned target_height,
	uint32_t **argb_data, int *width, int *height)
{
	char *path;
	char *allocated_path;
	char *options_end;
	char *render_opts;
	FRsvgHandle *rsvg;
	struct image_svg_dimensions {
		int width;
		int height;
		double em;
		double ex;
	} dim;
	CARD32 *data;
	Fcairo_surface_t *surface;
	Fcairo_t *cr;
	int i;
	size_t pixel_count;
	size_t pixel_index;
	int j;
	int b1;
	int b2;
	int w = 0;
	int h = 0;
	int dw = 0;
	int dh = 0;
	int w_sgn = 1;
	int h_sgn = 1;
	double angle = 0;
	double w_scale = 1;
	double h_scale = 1;
	double extra_w_scale = 1;
	double extra_h_scale = 1;
	double buf;
	Bool transpose = False;
	unsigned char a_value;
	unsigned char r_value;
	unsigned char g_value;
	unsigned char b_value;


	/* Separate rendering options from path */
	if (argb_data == NULL || width == NULL || height == NULL)
	{
		return False;
	}
	*argb_data = NULL;
	*width = 0;
	*height = 0;
	if (source == NULL || source->bytes == NULL || filename == NULL)
	{
		return False;
	}
	render_opts = path = allocated_path = strdup(filename);
	if (allocated_path == NULL)
	{
		return False;
	}
	options_end = *path == ':' ? strchr(path + 1, ':') : NULL;

	if (*path == ':' && options_end != NULL)
	{
		*options_end = 0;
		path = options_end + 1;
		render_opts ++;
	}
	else
	{
		render_opts = "";
	}

	if (!(rsvg = image_svg_from_input(source, path)))
	{
		free(allocated_path);

		return False;
	}

	/* Parsing of rendering options */
	while (*render_opts)
	{
		i = 0;
		switch (*render_opts)
		{
		case '!':
			transpose = !transpose;
			break;
		case '*':
			if (sscanf(render_opts, "*%lf%n", &buf, &i) >= 1)
			{
				switch (render_opts[i])
				{
				case 'x':
					w_scale *= buf;
					i ++;
					break;
				case 'y':
					h_scale *= buf;
					i ++;
					break;
				default:
					w_scale *= buf;
					h_scale *= buf;
				}
			}
			break;
		case '/':
			if (sscanf(render_opts, "/%lf%n", &buf, &i) >= 1 &&
			    buf)
			{
				switch (render_opts[i])
				{
				case 'x':
					w_scale /= buf;
					i ++;
					break;
				case 'y':
					h_scale /= buf;
					i ++;
					break;
				default:
					w_scale /= buf;
					h_scale /= buf;
				}
			}
			break;
		case '@':
			if (sscanf(render_opts, "@%lf%n", &buf, &i) >= 1)
			{
				angle += buf;
			}
			break;
		default:
			j = 0;
			int pair = image_svg_pair(render_opts, &b1, &b2, &j, &i);

			if (pair < 0)
			{
				free(allocated_path);
				Fg_object_unref(FG_OBJECT(rsvg));
				return False;
			}
			if (pair == 1)
			{
				if (b1 == INT_MIN || b2 == INT_MIN)
				{
					free(allocated_path);
					Fg_object_unref(FG_OBJECT(rsvg));
					return False;
				}
				w = b1;
				h = b2;
				if (w < 0 || (!w && render_opts[0] == '-'))
				{
					w *= (w_sgn = -1);
				}
				if (h < 0 || (!h && render_opts[j] == '-'))
				{
					h *= (h_sgn = -1);
				}
			}
			else if (pair == 2)
			{
				int64_t next_dw = (int64_t)dw + b1;
				int64_t next_dh = (int64_t)dh + b2;

				if (next_dw < INT_MIN || next_dw > INT_MAX ||
				    next_dh < INT_MIN || next_dh > INT_MAX)
				{
					free(allocated_path);
					Fg_object_unref(FG_OBJECT(rsvg));
					return False;
				}
				dw = (int)next_dw;
				dh = (int)next_dh;
			}
		}
		render_opts += i ? i : 1;
	}
	free(allocated_path);

	/* Keep the original aspect ratio when either w or h is 0 */
	double		ddw = 0, ddh = 0;
	gboolean	has_out_w, has_out_h, has_vb;
	RsvgLength	out_w, out_h;
	RsvgRectangle	viewbox;

	rsvg_handle_get_intrinsic_size_in_pixels(rsvg, &ddw, &ddh);

	rsvg_handle_get_intrinsic_dimensions(rsvg, &has_out_w, &out_w,
		&has_out_h, &out_h, &has_vb, &viewbox);

	if (!image_svg_dimension(has_vb ? viewbox.width : ddw, &dim.width) ||
	    !image_svg_dimension(has_vb ? viewbox.height : ddh, &dim.height))
	{
		Fg_object_unref(FG_OBJECT(rsvg));
		return False;
	}

	dim.em = dim.width;
	dim.ex = dim.height;
	if (has_out_w)
		dim.em = out_w.length <= dim.width ? dim.width : out_w.length;
	if (has_out_h)
		dim.ex = out_h.length <= dim.height ? dim.height : out_h.length;

	if (dim.width <= 0 || dim.height <= 0 || dim.em <= 0 || dim.ex <= 0 ||
	    !isfinite(dim.em) || !isfinite(dim.ex))
	{
		Fg_object_unref(FG_OBJECT(rsvg));
		return False;
	}
	if (target_width > 0 && target_height > 0)
	{
		if (target_width > (unsigned)INT_MAX ||
		    target_height > (unsigned)INT_MAX)
		{
			Fg_object_unref(FG_OBJECT(rsvg));
			return False;
		}
		w = (int)target_width;
		h = (int)target_height;
	}
	else if (!w && !h)
	{
		int scaled_w = 0;
		int scaled_h = 0;

		if (monitor_scale_apply(dim.width, factor, &scaled_w) !=
			MONITOR_SCALE_CONVERSION_OK ||
		    monitor_scale_apply(dim.height, factor, &scaled_h) !=
			MONITOR_SCALE_CONVERSION_OK)
		{
			Fg_object_unref(FG_OBJECT(rsvg));
			return False;
		}
		w = scaled_w;
		h = scaled_h;
	}
	else if (!w)
	{
		if (!image_svg_dimension(h * dim.em / dim.ex, &w))
		{
			Fg_object_unref(FG_OBJECT(rsvg));
			return False;
		}
	}
	else if (!h)
	{
		if (!image_svg_dimension(w * dim.ex / dim.em, &h))
		{
			Fg_object_unref(FG_OBJECT(rsvg));
			return False;
		}
	}

	extra_w_scale = w_scale;
	extra_h_scale = h_scale;
	w_scale *= w;
	h_scale *= h;

	if (transpose && dh == INT_MIN)
	{
		Fg_object_unref(FG_OBJECT(rsvg));
		return False;
	}
	if (transpose)
	{
		b1 = w;
		w = h;
		h = b1;

		b1 = w_sgn;
		w_sgn = - h_sgn;
		h_sgn = b1;

		b1 = dw;
		dw = - dh;
		dh = b1;

		angle += 90;
	}
	if (w <= 0 || h <= 0 ||
	    !FImageSizeIsSafe(
		    (uint64_t)w, (uint64_t)h, PIMAGE_MAX_DIMENSION,
		    PIMAGE_MAX_PIXELS, &pixel_count))
	{
		Fg_object_unref(FG_OBJECT(rsvg));
		return False;
	}


	data = calloc(pixel_count, sizeof(CARD32));
	if (data == NULL)
	{
		Fg_object_unref(FG_OBJECT(rsvg));
		return False;
	}
	surface = Fcairo_image_surface_create_for_data((unsigned char *)data,
		FCAIRO_FORMAT_ARGB32, w, h, w * sizeof(CARD32));
	if (Fcairo_surface_status(surface) != FCAIRO_STATUS_SUCCESS)
	{
		Fg_object_unref(FG_OBJECT(rsvg));
		free(data);
		if (surface)
		{
		   	Fcairo_surface_destroy(surface);
		}

		return False;
	}
	cr = Fcairo_create(surface);
	Fcairo_surface_destroy(surface);
	if (Fcairo_status(cr) != FCAIRO_STATUS_SUCCESS)
	{
		Fg_object_unref(FG_OBJECT(rsvg));
		free(data);
		if (cr)
		{
			Fcairo_destroy(cr);
		}

		return False;
	}

	GError *err = NULL;
	RsvgRectangle vp;

	/* Render once into the destination viewport. Extra * / path
	 * scales, mirrors, and rotation stay as cairo drawing ops. */
	Fcairo_translate(cr, .5 * w, .5 * h);
	Fcairo_scale(cr, w_sgn * extra_w_scale, h_sgn * extra_h_scale);
	Fcairo_translate(cr, dw, dh);
	Fcairo_rotate(cr, angle * M_PI / 180);
	Fcairo_translate(cr, -.5 * w, -.5 * h);

	vp.x = 0;
	vp.y = 0;
	vp.width = w;
	vp.height = h;

	rsvg_handle_render_document(rsvg, cr, &vp, &err);
	if (err != NULL)
	{
		g_error_free(err);
	}
	Fg_object_unref(FG_OBJECT(rsvg));
	Fcairo_destroy(cr);

	/* Cairo gave us alpha prescaled RGB values, hence we need
	 * to rescale them for PImageCreatePixmapFromArgbData() */
	for (pixel_index = 0; pixel_index < pixel_count; pixel_index++)
	{
		if ((a_value = (data[pixel_index] >> 030) & 0xff))
		{
			r_value = ((data[pixel_index] >> 020) & 0xff) * 0xff /
				a_value;
			g_value = ((data[pixel_index] >> 010) & 0xff) * 0xff /
				a_value;
			b_value =  (data[pixel_index]         & 0xff) * 0xff /
				a_value;

			data[pixel_index] =
				((CARD32)a_value << 24) |
				((CARD32)r_value << 16) |
				((CARD32)g_value << 8) | b_value;
		}
	}

	*width = w;
	*height = h;
	*argb_data = data;

	return True;
}
#endif
