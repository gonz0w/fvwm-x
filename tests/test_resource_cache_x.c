/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/Flocale.h"
#include "libs/Picture.h"
#include "libs/PictureBase.h"

#include <X11/Xutil.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int count_low_descriptors(void)
{
	int count = 0;

	for (int fd = 0; fd < 256; fd++)
	{
		if (fcntl(fd, F_GETFD) >= 0 || errno != EBADF)
		{
			count++;
		}
	}
	return count;
}

static void test_image_refusal(Display *display, char *path)
{
	FvwmPictureAttributes attributes = {0};
	monitor_scale_factor scale = {4};
	int before = count_low_descriptors();

	for (int attempt = 0; attempt < 16; attempt++)
	{
		Pixmap picture = 123, mask = 123, alpha = 123;
		int width = 123, height = 123, depth = 123;
		int nalloc_pixels = 123, no_limit = 123;
		Pixel *alloc_pixels = (Pixel *)&nalloc_pixels;

		assert(!PImageLoadPixmapFromFile(display,
			DefaultRootWindow(display), path, &picture, &mask,
			&alpha, &width, &height, &depth, &nalloc_pixels,
			&alloc_pixels, &no_limit, attributes));
		assert(picture == None && mask == None && alpha == None);
		assert(width == 0 && height == 0 && depth == 0);
		assert(nalloc_pixels == 0 && alloc_pixels == NULL);
		assert(no_limit == False);
		assert(PImageLoadFvwmPictureFromFile(display,
			DefaultRootWindow(display), path, attributes, scale,
			0, 0) == NULL);
	}
	assert(count_low_descriptors() == before);
}

static void test_picture_cache(Display *display, const char *path)
{
	FvwmPictureAttributes attributes = {0};
	monitor_scale_factor base_scale = {4}, double_scale = {8};
	Window root = DefaultRootWindow(display);
	char *same_path = malloc(strlen(path) + 1);

	assert(same_path);
	strcpy(same_path, path);
	FvwmPicture *base = PCacheFvwmPictureAtScale(display, root, NULL,
		same_path, attributes, base_scale, 0, 0, 7);
	FvwmPicture *hit = PCacheFvwmPictureAtScale(display, root, NULL,
		path, attributes, base_scale, 0, 0, 7);
	FvwmPicture *scaled = PCacheFvwmPictureAtScale(display, root, NULL,
		path, attributes, double_scale, 0, 0, 7);
	FvwmPicture *generation = PCacheFvwmPictureAtScale(display, root,
		NULL, path, attributes, base_scale, 0, 0, 8);
	FvwmPicture *target = PCacheFvwmPictureAtScale(display, root, NULL,
		path, attributes, base_scale, 16, 12, 7);
	FvwmPicture *target_height = PCacheFvwmPictureAtScale(display,
		root, NULL, path, attributes, base_scale, 16, 14, 7);
	FvwmPicture *target_width = PCacheFvwmPictureAtScale(display,
		root, NULL, path, attributes, base_scale, 18, 12, 7);

	assert(base && hit && scaled && generation && target);
	assert(base == hit && base->count == 2);
	assert(base != scaled && base != generation && base != target);
	assert(base->width == 8 && base->height == 8);
	assert(scaled->width == 16 && scaled->height == 16);
	assert(target->width == 16 && target->height == 12);
	assert(generation->width == 8 && generation->height == 8);
	assert(target_height && target_height != target);
	assert(target_height->width == 16 && target_height->height == 14);
	assert(target_width && target_width != target);
	assert(target_width->width == 18 && target_width->height == 12);
	PDestroyFvwmPicture(display, hit);
	assert(base->count == 1);
	PDestroyFvwmPicture(display, base);
	PDestroyFvwmPicture(display, scaled);
	PDestroyFvwmPicture(display, generation);
	PDestroyFvwmPicture(display, target);
	PDestroyFvwmPicture(display, target_height);
	PDestroyFvwmPicture(display, target_width);
	free(same_path);
}

static void test_bitmap_metadata(Display *display, const char *path)
{
	FvwmPictureAttributes attributes = {0};
	monitor_scale_factor scale = {4};
	FvwmPicture *picture = PCacheFvwmPictureAtScale(display,
		DefaultRootWindow(display), NULL, path, attributes, scale,
		0, 0, 7);

	assert(picture && picture->depth == 1);
	assert(picture->no_limit == False);
	assert(picture->nalloc_pixels == 0);
	PDestroyFvwmPicture(display, picture);
}

static void write_cache_bitmap(FILE *stream, unsigned value, Bool changed)
{
	assert(fputs("#define cache_width 8\n#define cache_height 8\n"
		"static unsigned char cache_bits[] = {\n", stream) >= 0);
	for (unsigned i = 0; i < 8; i++)
	{
		assert(fprintf(stream, "0x%02x%s", value,
			i == 7 ? "};\n" : ", ") > 0);
	}
	/* Size changes make reload independent of timestamp resolution. */
	if (changed)
	{
		assert(fputs("/* changed bitmap */\n", stream) >= 0);
	}
	assert(fclose(stream) == 0);
}

static unsigned long bitmap_pixel(Display *display, FvwmPicture *picture)
{
	XImage *image = XGetImage(display, picture->picture, 0, 0, 1, 1,
		AllPlanes, ZPixmap);
	unsigned long pixel;

	assert(image != NULL);
	pixel = XGetPixel(image, 0, 0);
	XDestroyImage(image);
	return pixel;
}

static void test_picture_reload(Display *display)
{
	char path[] = "/tmp/fvwm-picture-cache-XXXXXX";
	FvwmPictureAttributes attributes = {0};
	monitor_scale_factor scale = {4};
	Window root = DefaultRootWindow(display);
	int fd = mkstemp(path);
	FILE *stream;
	FvwmPicture *old, *other_generation, *replacement, *hit;

	assert(fd >= 0);
	stream = fdopen(fd, "w");
	assert(stream != NULL);
	write_cache_bitmap(stream, 255, False);
	old = PCacheFvwmPictureAtScale(display, root, NULL, path,
		attributes, scale, 0, 0, 7);
	other_generation = PCacheFvwmPictureAtScale(display, root, NULL,
		path, attributes, scale, 0, 0, 8);
	assert(old && other_generation && old != other_generation);
	assert(bitmap_pixel(display, old) == 1);
	stream = fopen(path, "w");
	assert(stream != NULL);
	write_cache_bitmap(stream, 0, True);
	replacement = PCacheFvwmPictureAtScale(display, root, NULL, path,
		attributes, scale, 0, 0, 7);
	assert(replacement && replacement != old);
	assert(replacement->stamp != old->stamp);
	assert(bitmap_pixel(display, replacement) == 0);
	assert(bitmap_pixel(display, old) == 1);
	assert(bitmap_pixel(display, other_generation) == 1);
	hit = PCacheFvwmPictureAtScale(display, root, NULL, path,
		attributes, scale, 0, 0, 7);
	assert(hit == replacement && replacement->count == 2);
	assert(unlink(path) == 0);
	assert(PCacheFvwmPictureAtScale(display, root, NULL, path,
		attributes, scale, 0, 0, 7) == NULL);
	assert(bitmap_pixel(display, old) == 1);
	assert(bitmap_pixel(display, replacement) == 0);
	PDestroyFvwmPicture(display, hit);
	PDestroyFvwmPicture(display, replacement);
	PDestroyFvwmPicture(display, other_generation);
	PDestroyFvwmPicture(display, old);
}

static void test_font_cache(Display *display)
{
	char name[] = "xft:DejaVu Sans:pixelsize=10";
	char same_name[] = "xft:DejaVu Sans:pixelsize=10";
	char module[] = "resource-cache-test";
	monitor_scale_factor base_scale = {4}, double_scale = {8};
	FlocaleFont *base = FlocaleLoadFontAtScale(display, name, module,
		base_scale, 7);
	FlocaleFont *hit = FlocaleLoadFontAtScale(display, same_name, module,
		base_scale, 7);
	FlocaleFont *scaled = FlocaleLoadFontAtScale(display, name, module,
		double_scale, 7);
	FlocaleFont *generation = FlocaleLoadFontAtScale(display, name,
		module, base_scale, 8);

	assert(base && hit && scaled && generation);
	assert(base == hit && base->count == 2);
	assert(base != scaled && base != generation);
	assert(scaled->height > base->height);
	assert(generation->height == base->height);
	FlocaleUnloadFont(display, hit);
	assert(base->count == 1);
	FlocaleUnloadFont(display, base);
	FlocaleUnloadFont(display, scaled);
	FlocaleUnloadFont(display, generation);
}

int main(int argc, char **argv)
{
	Display *display;

	assert(argc >= 3);
	display = XOpenDisplay(NULL);
	assert(display);
	PictureInitCMapRoot(display, True, NULL, False, True);
	FlocaleInit(LC_CTYPE, "C", "", "resource-cache-test");
	test_picture_cache(display, argv[1]);
	test_bitmap_metadata(display, argv[2]);
	test_picture_reload(display);
	test_font_cache(display);
	for (int i = 3; i < argc; i++)
	{
		test_image_refusal(display, argv[i]);
		test_picture_cache(display, argv[1]);
	}
	XSync(display, False);
	XCloseDisplay(display);
	puts("Image and font cache identity, scale, generation, "
	     "and release passed");
	return 0;
}
