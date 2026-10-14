/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/Picture.h"
#include "libs/PictureBase.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static enum {
	READ_ALLOCATION_FAILURE,
	INFO_ALLOCATION_FAILURE,
	CALLBACK_SETUP_ERROR
} failure;
static int read_calls, info_calls, callback_calls, destroy_calls;

png_structp
__real_png_create_read_struct(png_const_charp version, png_voidp context,
    png_error_ptr error, png_error_ptr warning);
png_infop
__real_png_create_info_struct(png_structp png);
void
__real_png_set_error_fn(png_structp png, png_voidp context,
    png_error_ptr error, png_error_ptr warning);
void
__real_png_destroy_read_struct(
    png_structpp png, png_infopp info, png_infopp end);

png_structp
__wrap_png_create_read_struct(png_const_charp version, png_voidp context,
    png_error_ptr error, png_error_ptr warning)
{
	read_calls++;
	/* No application jump destination exists during construction. */
	assert(context == NULL && error == NULL && warning == NULL);
	return failure == READ_ALLOCATION_FAILURE
		   ? NULL
		   : __real_png_create_read_struct(
			 version, context, error, warning);
}

png_infop
__wrap_png_create_info_struct(png_structp png)
{
	info_calls++;
	assert(png_get_error_ptr(png) == NULL);
	return failure == INFO_ALLOCATION_FAILURE
		   ? NULL
		   : __real_png_create_info_struct(png);
}

void
__wrap_png_set_error_fn(png_structp png, png_voidp context,
    png_error_ptr error, png_error_ptr warning)
{
	callback_calls++;
	assert(context && error);
	__real_png_set_error_fn(png, context, error, warning);
	/* Exercise the first point at which our callback may run. */
	png_error(png, "fixture setup error");
}

void
__wrap_png_destroy_read_struct(
    png_structpp png, png_infopp info, png_infopp end)
{
	destroy_calls++;
	__real_png_destroy_read_struct(png, info, end);
}

static int
count_low_descriptors(void)
{
	int count = 0;
	for (int fd = 0; fd < 256; fd++) {
		if (fcntl(fd, F_GETFD) >= 0 || errno != EBADF) {
			count++;
		}
	}
	return count;
}

static void
test_constructor_failure(char *path)
{
	FvwmPictureAttributes attributes  = {0};
	int		      descriptors = count_low_descriptors();

	for (int attempt = 0; attempt < 16; attempt++) {
		Pixmap picture = 123, mask = 123, alpha = 123;
		int    width = 123, height = 123, depth = 123;
		int    nalloc_pixels = 123, no_limit = 123;
		Pixel *alloc_pixels = (Pixel *)&nalloc_pixels;

		read_calls = info_calls = callback_calls = destroy_calls = 0;
		/* A signature-only file cannot reach any X bitmap/rendering
		 * path. */
		assert(!PImageLoadPixmapFromFile(NULL, None, path, &picture,
		    &mask, &alpha, &width, &height, &depth, &nalloc_pixels,
		    &alloc_pixels, &no_limit, attributes));
		assert(picture == None && mask == None && alpha == None);
		assert(width == 0 && height == 0 && depth == 0);
		assert(nalloc_pixels == 0 && alloc_pixels == NULL);
		assert(no_limit == False);
		assert(read_calls == 1);
		assert(info_calls == (failure != READ_ALLOCATION_FAILURE));
		assert(callback_calls == (failure == CALLBACK_SETUP_ERROR));
		assert(destroy_calls == (failure != READ_ALLOCATION_FAILURE));
	}
	assert(count_low_descriptors() == descriptors);
}

int
main(void)
{
	char		    path[]	= "/tmp/fvwm-png-setup-XXXXXX";
	const unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
	int		    fd		= mkstemp(path);

	assert(fd >= 0);
	assert(write(fd, signature, sizeof(signature)) == sizeof(signature));
	assert(close(fd) == 0);
	failure = READ_ALLOCATION_FAILURE;
	test_constructor_failure(path);
	failure = INFO_ALLOCATION_FAILURE;
	test_constructor_failure(path);
	failure = CALLBACK_SETUP_ERROR;
	test_constructor_failure(path);
	assert(unlink(path) == 0);
	return 0;
}
