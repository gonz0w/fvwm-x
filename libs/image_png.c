/* PNG decoding derived from PictureImageLoader.c:
 * Copyright (C) 2002 Olivier Chapuis; original imlib2 loader by
 * Carsten Haitzler and contributors.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "image_png.h"
#include "ImageSafety.h"
#include "image_svg_protocol.h"
#include <limits.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_PNG
#include <png.h>
typedef struct
{
	jmp_buf jump;
	uint32_t *data;
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

bool image_png_decode(const image_input *input, uint32_t **pixels,
	int *width, int *height)
{
	png_uint_32 w32, h32;
	png_structp png_ptr = NULL;
	png_infop Finfo_ptr = NULL;
	uint32_t *data;
	int w, h;
	char hasa, hasg;
	int bit_depth;
	int color_type;
	int interlace_type;
	unsigned char **lines;
	pimage_png_read *read;
	int i;
	size_t pixel_count;

	if (!pixels || !width || !height)
		return false;
	*pixels = NULL;
	*width = *height = 0;
	if (!input || input->length < 4 || png_sig_cmp(input->bytes, 0, 4) != 0)
		return false;
	read = calloc(1, sizeof(*read));
	if (!read)
		return false;
	read->input = input;
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
					   NULL, NULL, NULL);

	if (!png_ptr)
	{
		free(read);
		return false;
	}
	Finfo_ptr = png_create_info_struct(png_ptr);
	if (!Finfo_ptr)
	{
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		free(read);
		return false;
	}
	if (setjmp(read->jump))
	{
		free(read->lines);
		free(read->data);
		png_destroy_read_struct(&png_ptr, &Finfo_ptr, NULL);
		free(read);
		return false;
	}
	/* Construction uses libpng's own recovery. Install our callback only
	 * after its destination has been armed. */
	png_set_error_fn(png_ptr, read, pimage_png_error, NULL);
	png_set_read_fn(png_ptr, read, pimage_png_read_data);
	png_read_info(png_ptr, Finfo_ptr);
	png_get_IHDR(
		png_ptr, Finfo_ptr, (png_uint_32 *) (&w32),
		(png_uint_32 *) (&h32), &bit_depth, &color_type,
		&interlace_type, NULL, NULL);
	if (w32 > (png_uint_32)INT_MAX || h32 > (png_uint_32)INT_MAX ||
	    !FImageSizeIsSafe(
		    w32, h32, IMAGE_SVG_MAX_DIMENSION, IMAGE_SVG_MAX_PIXELS,
		    &pixel_count))
	{
		png_destroy_read_struct(&png_ptr, &Finfo_ptr, NULL);
		free(read);
		return false;
	}
	interlace_type = 0; /* not used */
	w = (int) w32;
	h = (int) h32;
	if (color_type == PNG_COLOR_TYPE_PALETTE)
	{
		png_set_expand(png_ptr);
	}

	hasa = color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA;
	hasg = color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA;

	if (hasa)
		png_set_expand(png_ptr);
	/* we want ARGB */
	/* note form raster:
	* thanks to mustapha for helping debug this on PPC Linux remotely by
	* sending across screenshots all the time and me figuring out form them
	* what the hell was up with the colors
	* now png loading should work on big endian machines nicely   */
#ifdef WORDS_BIGENDIAN
	png_set_swap_alpha(png_ptr);
	png_set_filler(png_ptr, 0xff, PNG_FILLER_BEFORE);
#else
	png_set_bgr(png_ptr);
	png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
#endif
	/* 16bit color -> 8bit color */
	png_set_strip_16(png_ptr);
	/* pack all pixels to byte boundaires */
	png_set_packing(png_ptr);
	if (png_get_valid(png_ptr, Finfo_ptr, PNG_INFO_tRNS))
	{
		png_set_expand(png_ptr);
	}

	data = calloc(pixel_count, sizeof(uint32_t));
	lines = calloc((size_t)h, sizeof(unsigned char *));
	read->data = data;
	read->lines = lines;
	if (!data || !lines)
		png_error(png_ptr, "allocation failed");

	if (hasg)
	{
		png_set_gray_to_rgb(png_ptr);
		if (png_get_bit_depth(png_ptr, Finfo_ptr) < 8)
		{
#if PNG_LIBPNG_VER >= 10400
			png_set_expand_gray_1_2_4_to_8(png_ptr);
#else
			png_set_gray_1_2_4_to_8(png_ptr);
#endif
		}
	}
	for (i = 0; i < h; i++)
	{
		lines[i] = (unsigned char *)data + (i * w * sizeof(uint32_t));
	}
	png_read_image(png_ptr, lines);
	png_read_end(png_ptr, Finfo_ptr);
	png_destroy_read_struct(&png_ptr, &Finfo_ptr, (png_infopp) NULL);
	free(read);
	free(lines);
	*width = w;
	*height = h;
	*pixels = data;
	return true;
}
#else
bool image_png_decode(const image_input *input, uint32_t **pixels,
	int *width, int *height)
{
	(void)input;
	if (!pixels || !width || !height)
		return false;
	*pixels = NULL;
	*width = *height = 0;
	return false;
}
#endif
