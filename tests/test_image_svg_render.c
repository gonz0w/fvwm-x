/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_svg.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const char red[] =
	"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"2\" height=\"2\">"
	"<rect width=\"2\" height=\"2\" fill=\"red\"/></svg>";

static void
check(const char *svg, const char *path, unsigned quarters,
	unsigned target_width, unsigned target_height, int expected_width,
	int expected_height)
{
	image_input input = {(unsigned char *)svg, strlen(svg)};
	uint32_t *pixels = (uint32_t *)1;
	int width = -1, height = -1;
	bool success = image_svg_render(&input, path,
	    (monitor_scale_factor){quarters}, target_width, target_height,
	    &pixels, &width, &height);

	if (expected_width == 0)
	{
		assert(!success && pixels == NULL && width == 0 && height == 0);
		return;
	}
	assert(success && pixels != NULL);
	assert(width == expected_width && height == expected_height);
	/* SVG's default aspect-ratio policy letterboxes a nonsquare viewport. */
	assert(pixels[(size_t)(height / 2) * width + width / 2] == 0xffff0000U);
	if (width == height)
	{
		for (size_t i = 0; i < (size_t)width * (size_t)height; i++)
		{
			assert(pixels[i] == 0xffff0000U);
		}
	}
	else
	{
		assert(pixels[0] == 0);
	}
	free(pixels);
}

int
main(void)
{
	check(red, "/tmp/fvwm-svg-unit.svg", 4, 0, 0, 2, 2);
	check(red, "/tmp/fvwm-svg-unit.svg", 8, 0, 0, 4, 4);
	check(red, "/tmp/fvwm-svg-unit.svg", 4, 3, 5, 3, 5);
	check(red, ":4x0:/tmp/fvwm-svg-unit.svg", 4, 0, 0, 4, 4);
	check(red, ":0x4:/tmp/fvwm-svg-unit.svg", 4, 0, 0, 4, 4);
	check(red, ":-2x-2:/tmp/fvwm-svg-unit.svg", 4, 0, 0, 2, 2);
	check(red, ":!@90*2/2:/tmp/fvwm-svg-unit.svg", 4, 0, 0, 2, 2);
	check(red, ":-2147483648x2:/tmp/fvwm-svg-unit.svg", 4, 0, 0, 0, 0);
	check(red, ":2147483648x2:/tmp/fvwm-svg-unit.svg", 4, 0, 0, 0, 0);
	check(red, ":2x999999999999999999999999:/tmp/fvwm-svg-unit.svg",
	    4, 0, 0, 0, 0);
	check(red, ":+2147483647+0+1+0:/tmp/fvwm-svg-unit.svg", 4, 0, 0, 0, 0);
	check(red, ":!+0-2147483648:/tmp/fvwm-svg-unit.svg", 4, 0, 0, 0, 0);
	check(red, "/tmp/fvwm-svg-unit.svg", 4, 32769, 1, 0, 0);
	check("<svg xmlns=\"http://www.w3.org/2000/svg\" "
	    "width=\"2147483648\" height=\"2\"/>",
	    "/tmp/fvwm-svg-unit.svg", 4, 0, 0, 0, 0);
	check("<svg xmlns=\"http://www.w3.org/2000/svg\" "
	    "viewBox=\"0 0 1e100 2\"/>",
	    "/tmp/fvwm-svg-unit.svg", 4, 0, 0, 0, 0);
	check("incomplete", "/tmp/fvwm-svg-unit.svg", 4, 0, 0, 0, 0);
	return 0;
}
