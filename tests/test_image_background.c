/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/image_background.h"
#include "libs/image_png.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void test_solid_and_bounds(void)
{
	struct background_plan plan = {.flags = BACKGROUND_SOLID << 1,
		.rgb = 0x123456};
	uint32_t *pixels = NULL;

	assert(image_background_render(&plan, 3, 2, &pixels));
	for (unsigned i = 0; i < 6; i++)
		assert(pixels[i] == 0xff123456);
	free(pixels);
	pixels = (void *)1;
	assert(!image_background_render(&plan, 0, 2, &pixels) && !pixels);
	assert(!image_background_render(&plan, 32769, 2, &pixels));
	assert(!image_background_render(&plan, 32768, 32768, &pixels));
	plan.flags = 16;
	assert(!image_background_render(&plan, 2, 2, &pixels));
	plan.flags = 0;
	assert(!image_background_render(&plan, 2, 2, &pixels));
}

#if HAVE_PNG
static void test_png_cover_and_failure(void)
{
	/* 4x2 RGB: red/green/blue/white; yellow/magenta/cyan/black. */
	static const unsigned char bytes[] = {
		0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,
		0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x02,
		0x08,0x02,0x00,0x00,0x00,0xf0,0xca,0xea,0x34,0x00,0x00,0x00,
		0x17,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0xc0,
		0x00,0xc6,0x40,0xc0,0x00,0x42,0x0c,0x60,0x8a,0x81,0x01,0x00,
		0x9b,0x7e,0x0b,0xf5,0x98,0x8f,0x8c,0xf0,0x00,0x00,0x00,0x00,
		0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82
	};
	char path[] = "/tmp/fvwm-background-XXXXXX";
	int fd = mkstemp(path), width, height;
	image_input input = {.bytes = (unsigned char *)bytes,
		.length = sizeof(bytes)};
	uint32_t *source = NULL, *pixels = NULL;
	struct background_plan plan = {.flags = BACKGROUND_PNG << 1};

	assert(fd >= 0 && write(fd, bytes, sizeof(bytes)) == sizeof(bytes));
	assert(close(fd) == 0);
	assert(image_png_decode(&input, &source, &width, &height));
	assert(width == 4 && height == 2);
	strcpy(plan.path, path);
	assert(image_background_render(&plan, 2, 2, &pixels));
	/* Cover crops the outside columns, rather than stretching all four. */
	assert(source[1] == 0xff00ff00 && source[2] == 0xff0000ff);
	assert(source[5] == 0xffff00ff && source[6] == 0xff00ffff);
	for (unsigned y = 0; y < 2; y++)
		for (unsigned x = 0; x < 2; x++)
			assert(pixels[y * 2 + x] == source[y * 4 + x + 1]);
	free(pixels);
	free(source);
	assert(unlink(path) == 0);
	pixels = (void *)1;
	assert(!image_background_render(&plan, 2, 2, &pixels) && !pixels);
	input.length = 8;
	assert(!image_png_decode(&input, &source, &width, &height));
	assert(!source && width == 0 && height == 0);
}
#endif

int main(void)
{
	test_solid_and_bounds();
#if HAVE_PNG
	test_png_cover_and_failure();
#endif
	return 0;
}
